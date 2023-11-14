#include "Socket.hpp"

#if !SOUP_WASM

#if SOUP_LINUX
#include <fcntl.h>
#include <unistd.h> // close
#include <poll.h>
#include <sys/resource.h>

#include "signal.hpp"
#endif

#include "aes.hpp"
#include "Buffer.hpp"
#include "BufferRefWriter.hpp"
#include "Curve25519.hpp"
#include "dnsResolver.hpp"
#include "ecc.hpp"
#include "Exception.hpp"
#include "NamedCurves.hpp"
#include "netConfig.hpp"
#include "ObfusString.hpp"
#include "rand.hpp"
#include "SocketTlsHandshaker.hpp"
#include "time.hpp"
#include "TlsAlertDescription.hpp"
#include "TlsCertificate.hpp"
#include "TlsClientHello.hpp"
#include "TlsClientHelloExtEllipticCurves.hpp"
#include "TlsClientHelloExtServerName.hpp"
#include "TlsContentType.hpp"
#include "TlsEncryptedPreMasterSecret.hpp"
#include "TlsExtensionType.hpp"
#include "TlsHandshake.hpp"
#include "TlsRecord.hpp"
#include "TlsServerHello.hpp"
#include "TlsServerKeyExchange.hpp"
#include "TrustStore.hpp"

#define LOGGING false

#if LOGGING
#include "log.hpp"
#endif

namespace soup
{
#if !SOUP_WINDOWS
	static void sigpipe_handler_proc(int)
	{
	}
#endif

	Socket::Socket() noexcept
		: Worker(true)
	{
		onConstruct();
	}

	void Socket::onConstruct() noexcept
	{
#if SOUP_WINDOWS
		if (wsa_consumers++ == 0)
		{
			WSADATA wsaData;
			WORD wVersionRequested = MAKEWORD(2, 2);
			WSAStartup(wVersionRequested, &wsaData);
		}
#else
		if (!made_linux_not_suck_dick)
		{
			made_linux_not_suck_dick = true;

			signal::handle(SIGPIPE, &sigpipe_handler_proc);

			rlimit rlim;
			if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
			{
				rlim.rlim_cur = rlim.rlim_max;
				setrlimit(RLIMIT_NOFILE, &rlim);
			}
		}
#endif
	}

	Socket::~Socket() noexcept
	{
		close();
#if SOUP_WINDOWS
		if (--wsa_consumers == 0)
		{
			WSACleanup();
		}
#endif
	}

	bool Socket::init(int af, int type)
	{
		if (fd == -1)
		{
			fd = ::socket(af, type, type == SOCK_STREAM ? IPPROTO_TCP : /* SOCK_DGRAM -> */ IPPROTO_UDP);
		}
		return fd != -1;
	}

	bool Socket::connect(const char* host, uint16_t port) noexcept
	{
		return connect(std::string(host), port);
	}

	bool Socket::connect(const std::string& host, uint16_t port) noexcept
	{
		if (IpAddr hostaddr; hostaddr.fromString(host))
		{
			return connect(hostaddr, port);
		}
		auto res = netConfig::get().dns_resolver->lookupIPv4(host);
		if (!res.empty() && connect(rand(res), port))
		{
			return true;
		}
		res = netConfig::get().dns_resolver->lookupIPv6(host);
		if (!res.empty() && connect(rand(res), port))
		{
			return true;
		}
		return false;
	}

	bool Socket::connect(const SocketAddr& addr) noexcept
	{
		SOUP_IF_UNLIKELY (!kickOffConnect(addr))
		{
			return false;
		}
		pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;
#if SOUP_WINDOWS
		int res = ::WSAPoll(&pfd, 1, netConfig::get().connect_timeout_ms);
#else
		int res = ::poll(&pfd, 1, netConfig::get().connect_timeout_ms);
#endif
		SOUP_IF_UNLIKELY (res != 1)
		{
			transport_close();
			return false;
		}
		return true;
	}

	bool Socket::connect(const IpAddr& ip, uint16_t port) noexcept
	{
		return connect(SocketAddr(ip, native_u16_t(port)));
	}

	bool Socket::kickOffConnect(const SocketAddr& addr) noexcept
	{
		peer = addr;
		if (addr.ip.isV4())
		{
			if (!init(AF_INET, SOCK_STREAM)
				|| !setNonBlocking()
				)
			{
				return false;
			}
			sockaddr_in sa{};
			sa.sin_family = AF_INET;
			sa.sin_port = addr.port;
			sa.sin_addr.s_addr = addr.ip.getV4();
			::connect(fd, (sockaddr*)&sa, sizeof(sa));
		}
		else
		{
			if (!init(AF_INET6, SOCK_STREAM)
				|| !setNonBlocking()
				)
			{
				return false;
			}
			sockaddr_in6 sa{};
			sa.sin6_family = AF_INET6;
			sa.sin6_port = addr.port;
			memcpy(&sa.sin6_addr, &addr.ip.data, sizeof(in6_addr));
			::connect(fd, (sockaddr*)&sa, sizeof(sa));
		}
		return true;
	}

	bool Socket::kickOffConnect(const IpAddr& ip, uint16_t port) noexcept
	{
		return kickOffConnect(SocketAddr(ip, native_u16_t(port)));
	}

	bool Socket::isPortLocallyBound(uint16_t port)
	{
		auto og = netConfig::get().connect_timeout_ms;
		netConfig::get().connect_timeout_ms = 20;
		Socket sock;
		const bool ret = sock.connect(IpAddr(SOUP_IPV4(127, 0, 0, 1)), port);
		netConfig::get().connect_timeout_ms = og;
		return ret;
	}

	bool Socket::bind6(uint16_t port) noexcept
	{
		return bind6(SOCK_STREAM, port);
	}

	bool Socket::bind4(uint16_t port) noexcept
	{
		return bind4(SOCK_STREAM, port);
	}

	bool Socket::udpBind6(uint16_t port) noexcept
	{
		return bind6(SOCK_DGRAM, port);
	}

	bool Socket::udpBind4(uint16_t port) noexcept
	{
		return bind4(SOCK_DGRAM, port);
	}

	bool Socket::udpBind(const IpAddr& addr, uint16_t port) noexcept
	{
		return addr.isV4()
			? bind4(SOCK_DGRAM, port, addr)
			: bind6(SOCK_DGRAM, port, addr)
			;
	}

	bool Socket::bind6(int type, uint16_t port, const IpAddr& addr) noexcept
	{
		if (!init(AF_INET6, type))
		{
			return false;
		}

		const auto port_ne = Endianness::toNetwork(native_u16_t(port));

		peer.ip.reset();
		peer.port = port_ne;

		sockaddr_in6 sa{};
		sa.sin6_family = AF_INET6;
		sa.sin6_port = port_ne;
		memcpy(&sa.sin6_addr, &addr.data, sizeof(in6_addr));
		return setOpt<int>(SO_REUSEADDR, 1)
			&& bind(fd, (sockaddr*)&sa, sizeof(sa)) != -1
			&& (type != SOCK_STREAM || listen(fd, 100) != -1)
			&& setNonBlocking()
			;
	}

	bool Socket::bind4(int type, uint16_t port, const IpAddr& addr) noexcept
	{
		if (!init(AF_INET, type))
		{
			return false;
		}

		const auto port_ne = Endianness::toNetwork(native_u16_t(port));

		peer.ip.reset();
		peer.port = port_ne;

		sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = port_ne;
		sa.sin_addr.s_addr = addr.getV4();
		return setOpt<int>(SO_REUSEADDR, 1)
			&& bind(fd, (sockaddr*)&sa, sizeof(sa)) != -1
			&& (type != SOCK_STREAM || listen(fd, 100) != -1)
			&& setNonBlocking()
			;
	}

#if SOUP_WINDOWS
	using socklen_t = int;
#endif

	Socket Socket::accept6() noexcept
	{
		Socket res{};
		sockaddr_in6 addr;
		socklen_t addrlen = sizeof(addr);
		res.fd = ::accept(fd, (sockaddr*)&addr, &addrlen);
		if (res.hasConnection())
		{
			memcpy(&res.peer.ip, &addr.sin6_addr, sizeof(addr.sin6_addr));
			res.peer.port = addr.sin6_port;
		}
		return res;
	}

	Socket Socket::accept4() noexcept
	{
		Socket res{};
		sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		res.fd = ::accept(fd, (sockaddr*)&addr, &addrlen);
		if (res.hasConnection())
		{
			res.peer.ip = network_u32_t(addr.sin_addr.s_addr);
			res.peer.port = addr.sin_port;
		}
		return res;
	}

	bool Socket::setBlocking(bool blocking) noexcept
	{
#if SOUP_WINDOWS
		unsigned long mode = blocking ? 0 : 1;
		return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1) return false;
		flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
		return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
	}

	bool Socket::setNonBlocking() noexcept
	{
		return setBlocking(false);
	}

	bool Socket::certchain_validator_none(const X509Certchain&, const std::string&)
	{
		return true;
	}

	bool Socket::certchain_validator_relaxed(const X509Certchain& chain, const std::string& domain)
	{
		return chain.certs.at(0).valid_to >= time::unixSeconds()
			&& chain.verify(domain, TrustStore::fromMozilla())
			;
	}

	bool Socket::certchain_validator_strict(const X509Certchain& chain, const std::string& domain)
	{
		return chain.canBeVerified()
			&& certchain_validator_relaxed(chain, domain)
			;
	}

	template <typename T>
	static void vector_emplace_back_randomised(std::vector<T>& target, std::vector<T>&& values)
	{
		while (!values.empty())
		{
			size_t i = rand(0, values.size() - 1);
			target.emplace_back(values.at(i));
			values.erase(values.begin() + i);
		}
	}

	[[nodiscard]] static TlsCipherSuite_t tls_randGreaseyCiphersuite()
	{
		switch (rand(0, 15))
		{
		case 0: return TLS_GREASE_0;
		case 1: return TLS_GREASE_1;
		case 2: return TLS_GREASE_2;
		case 3: return TLS_GREASE_3;
		case 4: return TLS_GREASE_4;
		case 5: return TLS_GREASE_5;
		case 6: return TLS_GREASE_6;
		case 7: return TLS_GREASE_7;
		case 8: return TLS_GREASE_8;
		case 9: return TLS_GREASE_9;
		case 10: return TLS_GREASE_10;
		case 11: return TLS_GREASE_11;
		case 12: return TLS_GREASE_12;
		case 13: return TLS_GREASE_13;
		case 14: return TLS_GREASE_14;
		default:;
		}
		return TLS_GREASE_15;
	}

	struct CaptureValidateCertchain
	{
		Socket& s;
		SocketTlsHandshaker* handshaker;
		certchain_validator_t certchain_validator;
	};

	void Socket::enableCryptoClient(std::string server_name, void(*callback)(Socket&, Capture&&), Capture&& cap)
	{
		auto handshaker = make_unique<SocketTlsHandshaker>(
			callback,
			std::move(cap)
		);
		handshaker->server_name = std::move(server_name);

		TlsClientHello hello;
		hello.random.time = static_cast<uint32_t>(time::unixSeconds());
		rand.fill(hello.random.random);
		handshaker->client_random = hello.random.toBinaryString();
		vector_emplace_back_randomised(hello.cipher_suites, {
			TLS_RSA_WITH_AES_256_CBC_SHA256,
			TLS_RSA_WITH_AES_128_CBC_SHA256,
			TLS_RSA_WITH_AES_256_CBC_SHA,
			TLS_RSA_WITH_AES_128_CBC_SHA,
		});
		vector_emplace_back_randomised(hello.cipher_suites, {
			TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
			TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, // Cloudfront
			TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
		});
		vector_emplace_back_randomised(hello.cipher_suites, {
			TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, // Cloudflare
			TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, // Cloudflare
			TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, // Cloudflare
		});
		vector_emplace_back_randomised(hello.cipher_suites, {
			TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, // Apache + Let's Encrypt
			TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, // Apache + Let's Encrypt
		});
		hello.cipher_suites.emplace(
			hello.cipher_suites.begin() + rand(0, hello.cipher_suites.size() - 1),
			tls_randGreaseyCiphersuite()
		);
		hello.compression_methods = { 0 };

		{
			TlsClientHelloExtServerName ext_server_name{};
			ext_server_name.host_name = handshaker->server_name;

			hello.extensions.add(TlsExtensionType::server_name, ext_server_name);
		}

		{
			TlsClientHelloExtEllipticCurves ext_elliptic_curves{};
			ext_elliptic_curves.named_curves = {
				NamedCurves::x25519,
				NamedCurves::secp256r1,
				NamedCurves::secp384r1,
			};

			hello.extensions.add(TlsExtensionType::elliptic_curves, ext_elliptic_curves);
		}

		// RFC 8422 says we MUST send this if we're compliant, but this isn't practically needed, so we're using it to defend against JA3 fingerprinting.
		if (soup::rand.coinflip())
		{
			hello.extensions.add(TlsExtensionType::ec_point_formats, std::string("\x01\x00", 2));
		}

		{
			// NGINX/OpenSSL (e.g., api.deepl.com) closes with "internal_error" if signature_algorithms is not present. We provide the following:
			// - ecdsa_secp256r1_sha256 (0x0403)
			// - rsa_pss_rsae_sha256 (0x0804)
			// - rsa_pkcs1_sha256 (0x0401)
			// - ecdsa_secp384r1_sha384 (0x0503)
			// - rsa_pss_rsae_sha384 (0x0805)
			// - rsa_pkcs1_sha384 (0x0501)
			// - rsa_pss_rsae_sha512 (0x0806)
			// - rsa_pkcs1_sha512 (0x0601)
			hello.extensions.add(TlsExtensionType::signature_algorithms, std::string("\x00\x10\x04\x03\x08\x04\x04\x01\x05\x03\x08\x05\x05\x01\x08\x06\x06\x01", 18));
		}

		// We support only TLS 1.2. Not particularly useful to provide this extension, but in the future we may support TLS 1.3 and then
		// this would defend against downgrade attacks.
		// For now, we can use it to defend against JA3 fingerprinting. :^)
		if (soup::rand.coinflip())
		{
			hello.extensions.add(TlsExtensionType::supported_versions, "\x02\x03\x03");
		}

		if (tls_sendHandshake(handshaker, TlsHandshake::client_hello, hello.toBinaryString()))
		{
			tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
			{
				if (handshake_type != TlsHandshake::server_hello)
				{
					s.tls_close(TlsAlertDescription::unexpected_message);
					return;
				}
				TlsServerHello shello;
				if (!shello.fromBinary(data))
				{
					s.tls_close(TlsAlertDescription::decode_error);
					return;
				}
				handshaker->cipher_suite = shello.cipher_suite;
				handshaker->server_random = shello.random.toBinaryString();

				s.tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
				{
					if (handshake_type != TlsHandshake::certificate)
					{
						s.tls_close(TlsAlertDescription::unexpected_message);
						return;
					}
					TlsCertificate cert;
					if (!cert.fromBinary(data))
					{
						s.tls_close(TlsAlertDescription::decode_error);
						return;
					}
					if (!handshaker->certchain.fromDer(cert.asn1_certs))
					{
						s.tls_close(TlsAlertDescription::bad_certificate);
						return;
					}
					handshaker->certchain.cleanup();

					// Validating an ECC cert on my i9-13900K takes around 61 ms, which is time the scheduler could be spending doing more useful things.
					handshaker->promise.fulfilOffThread([](Capture&& _cap)
					{
						auto& cap = _cap.get<CaptureValidateCertchain>();
						if (!cap.certchain_validator(cap.handshaker->certchain, cap.handshaker->server_name))
						{
							cap.s.tls_close(TlsAlertDescription::bad_certificate);
						}
					}, CaptureValidateCertchain{
						s,
						handshaker.get(),
						netConfig::get().certchain_validator
					});

					auto* p = &handshaker->promise;
					s.awaitPromiseCompletion(p, [](Worker& w, Capture&& cap)
					{
						w.holdup_type = Worker::NONE;

						auto& s = static_cast<Socket&>(w);
						UniquePtr<SocketTlsHandshaker> handshaker = std::move(cap.get<UniquePtr<SocketTlsHandshaker>>());

						switch (handshaker->cipher_suite)
						{
						default:
							s.enableCryptoClientRecvServerHelloDone(std::move(handshaker));
							break;

						case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
						case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
						case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
						case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
						case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
						case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
						case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
						case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
							s.tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
							{
								if (handshake_type != TlsHandshake::server_key_exchange)
								{
									s.tls_close(TlsAlertDescription::unexpected_message);
									return;
								}
								TlsServerKeyExchange ske;
								if (!ske.fromBinary(data))
								{
									s.tls_close(TlsAlertDescription::decode_error);
									return;
								}
								if (ske.named_curve == NamedCurves::x25519)
								{
									if (ske.point.size() != Curve25519::KEY_SIZE)
									{
										s.tls_close(TlsAlertDescription::handshake_failure);
										return;
									}
								}
								else if (ske.named_curve == NamedCurves::secp256r1)
								{
									if (ske.point.size() != 65 // first byte for compression format (4 for uncompressed) + 32 for X + 32 for Y
										|| ske.point.at(0) != 4
										)
									{
										s.tls_close(TlsAlertDescription::handshake_failure);
										return;
									}
									ske.point.erase(0, 1); // leave only X + Y
								}
								else if (ske.named_curve == NamedCurves::secp384r1)
								{
									if (ske.point.size() != 97 // first byte for compression format (4 for uncompressed) + 48 for X + 48 for Y
										|| ske.point.at(0) != 4
										)
									{
										s.tls_close(TlsAlertDescription::handshake_failure);
										return;
									}
									ske.point.erase(0, 1); // leave only X + Y
								}
								else
								{
									s.tls_close(TlsAlertDescription::handshake_failure);
									return;
								}
								// TODO: Validate signature
								handshaker->ecdhe_curve = ske.named_curve;
								handshaker->ecdhe_public_key = std::move(ske.point);

								s.enableCryptoClientRecvServerHelloDone(std::move(handshaker));
							});
							break;
						}
					}, std::move(handshaker));
				});
			});
		}
	}

	void Socket::enableCryptoClientRecvServerHelloDone(UniquePtr<SocketTlsHandshaker>&& handshaker)
	{
		tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
		{
			if (handshake_type == TlsHandshake::server_hello_done)
			{
				s.enableCryptoClientProcessServerHelloDone(std::move(handshaker));
			}
			else if (handshake_type == TlsHandshake::certificate_request)
			{
				s.tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
				{
					if (handshake_type != TlsHandshake::server_hello_done)
					{
						s.tls_close(TlsAlertDescription::unexpected_message);
						return;
					}

					TlsCertificate cert{};
					if (s.tls_sendHandshake(handshaker, TlsHandshake::certificate, cert.toBinaryString()))
					{
						s.enableCryptoClientProcessServerHelloDone(std::move(handshaker));
					}
				});
			}
			else
			{
				s.tls_close(TlsAlertDescription::unexpected_message);
			}
		});
	}

	void Socket::enableCryptoClientProcessServerHelloDone(UniquePtr<SocketTlsHandshaker>&& handshaker)
	{
		std::string cke{};
		if (handshaker->ecdhe_curve == 0)
		{
			std::string pms{};
			pms.reserve(48);
			pms.push_back('\3');
			pms.push_back('\3');
			for (auto i = 0; i != 46; ++i)
			{
				pms.push_back(rand.ch());
			}

			TlsEncryptedPreMasterSecret epms{};
			SOUP_IF_UNLIKELY (!handshaker->certchain.certs.at(0).isRsa())
			{
				SOUP_THROW(Exception(ObfusString("Server picked an RSA ciphersuite but did not provide an appropriate certificate").str()));
			}
			epms.data = handshaker->certchain.certs.at(0).getRsaPublicKey().encryptPkcs1(pms).toBinary();
			cke = epms.toBinaryString();

			handshaker->pre_master_secret = std::move(pms);
		}
		else if (handshaker->ecdhe_curve == NamedCurves::x25519)
		{
			uint8_t my_priv[Curve25519::KEY_SIZE];
			Curve25519::generatePrivate(my_priv);

			uint8_t their_pub[Curve25519::KEY_SIZE];
			memcpy(their_pub, handshaker->ecdhe_public_key.data(), sizeof(their_pub));

			uint8_t shared_secret[Curve25519::SHARED_SIZE];
			Curve25519::x25519(shared_secret, my_priv, their_pub);
			handshaker->pre_master_secret = std::string((const char*)shared_secret, sizeof(shared_secret));

			uint8_t my_pub[Curve25519::KEY_SIZE];
			Curve25519::derivePublic(my_pub, my_priv);

			cke = std::string(1, (char)sizeof(my_pub));
			cke.append((const char*)my_pub, sizeof(my_pub));
		}
		else if (handshaker->ecdhe_curve == NamedCurves::secp256r1
			|| handshaker->ecdhe_curve == NamedCurves::secp384r1
			)
		{
			const EccCurve* curve;
			if (handshaker->ecdhe_curve != NamedCurves::secp384r1)
			{
				curve = &EccCurve::secp256r1();
			}
			else
			{
				curve = &EccCurve::secp384r1();
			}
			size_t csize = curve->getBytesPerAxis();

			auto my_priv = curve->generatePrivate();

			EccPoint their_pub{
				Bigint::fromBinary(handshaker->ecdhe_public_key.substr(0, csize)),
				Bigint::fromBinary(handshaker->ecdhe_public_key.substr(csize, csize))
			};
			SOUP_IF_UNLIKELY (!curve->validate(their_pub))
			{
				SOUP_THROW(Exception(ObfusString("Server provided an invalid point for ECDHE").str()));
			}

			auto shared_point = curve->multiply(their_pub, my_priv);
			auto shared_secret = shared_point.x.toBinary(csize);
			handshaker->pre_master_secret = std::move(shared_secret);

			auto my_pub = curve->derivePublic(my_priv);

			cke = curve->encodePointUncompressed(my_pub);
			cke.insert(0, 1, static_cast<char>(1 + csize + csize));
		}
		else
		{
			SOUP_ASSERT_UNREACHABLE; // This would be a logic error on our end since we (should) reject other curves earlier
		}
		if (tls_sendHandshake(handshaker, TlsHandshake::client_key_exchange, std::move(cke))
			&& tls_sendRecord(TlsContentType::change_cipher_spec, "\1")
			)
		{
			handshaker->getKeys(
				tls_encrypter_send.mac_key,
				handshaker->pending_recv_encrypter.mac_key,
				tls_encrypter_send.cipher_key,
				handshaker->pending_recv_encrypter.cipher_key,
				tls_encrypter_send.implicit_iv,
				handshaker->pending_recv_encrypter.implicit_iv
			);
			if (tls_sendHandshake(handshaker, TlsHandshake::finished, handshaker->getClientFinishVerifyData()))
			{
				tls_recvRecord(TlsContentType::change_cipher_spec, [](Socket& s, std::string&& data, Capture&& cap)
				{
					UniquePtr<SocketTlsHandshaker> handshaker = std::move(cap.get<UniquePtr<SocketTlsHandshaker>>());

					s.tls_encrypter_recv = std::move(handshaker->pending_recv_encrypter);

					handshaker->expected_finished_verify_data = handshaker->getServerFinishVerifyData();

					s.tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
					{
						if (handshake_type != TlsHandshake::finished)
						{
							s.tls_close(TlsAlertDescription::unexpected_message);
							return;
						}
						if (data != handshaker->expected_finished_verify_data)
						{
							s.tls_close(TlsAlertDescription::decrypt_error);
							return;
						}
						handshaker->callback(s, std::move(handshaker->callback_capture));
					});
				}, std::move(handshaker));
			}
		}
	}

	[[nodiscard]] static bool tls_serverSupportsCipherSuite(uint16_t cs) noexcept
	{
		switch (cs)
		{
		case TLS_RSA_WITH_AES_128_CBC_SHA:
		case TLS_RSA_WITH_AES_256_CBC_SHA:
		case TLS_RSA_WITH_AES_128_CBC_SHA256:
		case TLS_RSA_WITH_AES_256_CBC_SHA256:
			return true;
		}
		return false;
	}

	struct CaptureDecryptPreMasterSecret
	{
		SocketTlsHandshaker* handshaker;
		Bigint data;
	};

	void Socket::enableCryptoServer(tls_server_cert_selector_t cert_selector, void(*callback)(Socket&, Capture&&), Capture&& cap, tls_server_on_client_hello_t on_client_hello)
	{
		auto handshaker = make_unique<SocketTlsHandshaker>(
			callback,
			std::move(cap)
		);
		handshaker->cert_selector = cert_selector;
		handshaker->on_client_hello = on_client_hello;
		tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
		{
			if (handshake_type != TlsHandshake::client_hello)
			{
				s.tls_close(TlsAlertDescription::unexpected_message);
				return;
			}

			{
				TlsClientHello hello;
				if (!hello.fromBinary(data))
				{
					s.tls_close(TlsAlertDescription::decode_error);
					return;
				}
				for (const auto& cs : hello.cipher_suites)
				{
					if (tls_serverSupportsCipherSuite(cs))
					{
						handshaker->cipher_suite = cs;
						break;
					}
				}

				std::string server_name{};
				for (const auto& ext : hello.extensions.extensions)
				{
					if (ext.id == TlsExtensionType::server_name)
					{
						TlsClientHelloExtServerName ext_server_name;
						if (ext_server_name.fromBinary(ext.data))
						{
							server_name = std::move(ext_server_name.host_name);
						}
						break;
					}
				}
				handshaker->cert_selector(handshaker->rsa_data, server_name);
				if (handshaker->rsa_data.der_encoded_certchain.empty())
				{
					s.tls_close(TlsAlertDescription::handshake_failure);
					return;
				}

				handshaker->client_random = hello.random.toBinaryString();

				if (handshaker->on_client_hello)
				{
					handshaker->on_client_hello(s, std::move(hello));
				}
			}

			{
				TlsServerHello shello{};
				shello.random.time = static_cast<uint32_t>(time::unixSeconds());
				rand.fill(shello.random.random);
				handshaker->server_random = shello.random.toBinaryString();
				shello.cipher_suite = handshaker->cipher_suite;
				shello.compression_method = 0;
				if (!s.tls_sendHandshake(handshaker, TlsHandshake::server_hello, shello.toBinaryString()))
				{
					return;
				}
			}

			{
				TlsCertificate tcert;
				tcert.asn1_certs = std::move(handshaker->rsa_data.der_encoded_certchain);
				if (!s.tls_sendHandshake(handshaker, TlsHandshake::certificate, tcert.toBinaryString()))
				{
					return;
				}
			}

			if (!s.tls_sendHandshake(handshaker, TlsHandshake::server_hello_done, {}))
			{
				return;
			}

			s.tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
			{
				if (handshake_type != TlsHandshake::client_key_exchange)
				{
					s.tls_close(TlsAlertDescription::unexpected_message);
					return;
				}

				if (data.size() <= 2)
				{
					s.tls_close(TlsAlertDescription::decode_error);
					return;
				}
				data.erase(0, 2); // length prefix

				handshaker->promise.fulfilOffThread([](Capture&& _cap)
				{
					auto& cap = _cap.get<CaptureDecryptPreMasterSecret>();
					cap.handshaker->pre_master_secret = cap.handshaker->rsa_data.private_key.decryptPkcs1(cap.data);
				}, CaptureDecryptPreMasterSecret{
					handshaker.get(),
					Bigint::fromBinary(data)
				});

				s.tls_recvRecord(TlsContentType::change_cipher_spec, [](Socket& s, std::string&& data, Capture&& cap)
				{
					if (!s.tls_sendRecord(TlsContentType::change_cipher_spec, "\1"))
					{
						return;
					}

					UniquePtr<SocketTlsHandshaker> handshaker = std::move(cap.get<UniquePtr<SocketTlsHandshaker>>());

					auto* p = &handshaker->promise;
					s.awaitPromiseCompletion(p, [](Worker& w, Capture&& cap)
					{
						w.holdup_type = Worker::NONE;

						auto& s = static_cast<Socket&>(w);
						UniquePtr<SocketTlsHandshaker> handshaker = std::move(cap.get<UniquePtr<SocketTlsHandshaker>>());

						handshaker->getKeys(
							s.tls_encrypter_recv.mac_key,
							s.tls_encrypter_send.mac_key,
							s.tls_encrypter_recv.cipher_key,
							s.tls_encrypter_send.cipher_key,
							s.tls_encrypter_recv.implicit_iv,
							s.tls_encrypter_send.implicit_iv
						);

						handshaker->expected_finished_verify_data = handshaker->getClientFinishVerifyData();

						s.tls_recvHandshake(std::move(handshaker), [](Socket& s, UniquePtr<SocketTlsHandshaker>&& handshaker, TlsHandshakeType_t handshake_type, std::string&& data)
						{
							if (handshake_type != TlsHandshake::finished)
							{
								s.tls_close(TlsAlertDescription::unexpected_message);
								return;
							}

							if (data != handshaker->expected_finished_verify_data)
							{
								s.tls_close(TlsAlertDescription::decrypt_error);
								return;
							}

							if (s.tls_sendHandshake(handshaker, TlsHandshake::finished, handshaker->getServerFinishVerifyData()))
							{
								handshaker->callback(s, std::move(handshaker->callback_capture));
							}
						});
					}, std::move(handshaker));
				}, std::move(handshaker));
			});
		});
	}

	bool Socket::isEncrypted() const noexcept
	{
		return tls_encrypter_send.isActive();
	}

	bool Socket::send(const std::string& data)
	{
		if (tls_encrypter_send.isActive())
		{
			return tls_sendRecordEncrypted(TlsContentType::application_data, data);
		}
		return transport_send(data);
	}

	bool Socket::initUdpBroadcast4()
	{
		return init(AF_INET, SOCK_DGRAM)
			&& setOpt<bool>(SO_BROADCAST, true)
			;
	}

	bool Socket::setSourcePort4(uint16_t port)
	{
		sockaddr_in bindto{};
		bindto.sin_family = AF_INET;
		bindto.sin_addr.s_addr = INADDR_ANY;
		bindto.sin_port = Endianness::toNetwork(native_u16_t(port));
		return ::bind(fd, (sockaddr*)&bindto, sizeof(bindto)) != -1;
	}

	bool Socket::udpClientSend(const SocketAddr& addr, const std::string& data) noexcept
	{
		peer = addr;
		return init(addr.ip.isV4() ? AF_INET : AF_INET6, SOCK_DGRAM)
			&& udpServerSend(addr, data)
			;
	}

	bool Socket::udpClientSend(const IpAddr& ip, uint16_t port, const std::string& data) noexcept
	{
		return udpClientSend(SocketAddr(ip, native_u16_t(port)), data);
	}

	bool Socket::udpServerSend(const SocketAddr& addr, const std::string& data) noexcept
	{
		if (addr.ip.isV4())
		{
			sockaddr_in sa{};
			sa.sin_family = AF_INET;
			sa.sin_port = addr.port;
			sa.sin_addr.s_addr = addr.ip.getV4();
			if (::sendto(fd, data.data(), static_cast<int>(data.size()), 0, (sockaddr*)&sa, sizeof(sa)) != data.size())
			{
				return false;
			}
		}
		else
		{
			sockaddr_in6 sa{};
			sa.sin6_family = AF_INET6;
			memcpy(&sa.sin6_addr, &addr.ip.data, sizeof(in6_addr));
			sa.sin6_port = addr.port;
			if (::sendto(fd, data.data(), static_cast<int>(data.size()), 0, (sockaddr*)&sa, sizeof(sa)) != data.size())
			{
				return false;
			}
		}
		return true;
	}

	bool Socket::udpServerSend(const IpAddr& ip, uint16_t port, const std::string& data) noexcept
	{
		return udpServerSend(SocketAddr(ip, native_u16_t(port)), data);
	}

	struct CaptureSocketRecv
	{
		void(*callback)(Socket&, std::string&&, Capture&&);
		Capture cap;
	};

	void Socket::recv(void(*callback)(Socket&, std::string&&, Capture&&), Capture&& cap)
	{
		CaptureSocketRecv inner_cap{
			callback,
			std::move(cap)
		};
		auto inner_callback = [](Socket& s, std::string&& data, Capture&& _cap)
		{
			auto& cap = _cap.get<CaptureSocketRecv>();
			cap.callback(s, std::move(data), std::move(cap.cap));
		};
		if (tls_encrypter_recv.isActive())
		{
			tls_recvRecord(TlsContentType::application_data, inner_callback, std::move(inner_cap));
		}
		else
		{
			transport_recv(0x1000, inner_callback, std::move(inner_cap));
		}
	}

	struct CaptureSocketUdpRecv
	{
		void(*callback)(Socket&, SocketAddr&&, std::string&&, Capture&&);
		Capture cap;
	};

	void Socket::udpRecv(void(*callback)(Socket&, SocketAddr&&, std::string&&, Capture&&), Capture&& cap)
	{
		holdup_type = SOCKET;
		holdup_callback.set([](Worker& w, Capture&& _cap)
		{
			w.holdup_type = Worker::NONE;

			auto& cap = _cap.get<CaptureSocketUdpRecv>();

			std::string data(0x1000, '\0');

			sockaddr_in6 sa;
			socklen_t sal = sizeof(sa);
			data.resize(::recvfrom(static_cast<Socket&>(w).fd, data.data(), 0x1000, 0, (sockaddr*)&sa, &sal));

			SocketAddr sender;
			if (sal == sizeof(sa))
			{
				sender.ip = IpAddr(reinterpret_cast<uint8_t*>(&sa.sin6_addr));
				sender.port = network_u16_t(sa.sin6_port);
			}
			else
			{
				sender.ip = network_u32_t(*reinterpret_cast<uint32_t*>(&reinterpret_cast<sockaddr_in*>(&sa)->sin_addr));
				sender.port = network_u16_t(reinterpret_cast<sockaddr_in*>(&sa)->sin_port);
			}

			cap.callback(static_cast<Socket&>(w), std::move(sender), std::move(data), std::move(cap.cap));
		}, CaptureSocketUdpRecv{ callback, std::move(cap) });
	}

	void Socket::close()
	{
		if (tls_encrypter_send.isActive())
		{
			tls_close(TlsAlertDescription::close_notify);
		}
		else
		{
			transport_close();
		}
	}

	bool Socket::tls_sendHandshake(const UniquePtr<SocketTlsHandshaker>& handshaker, TlsHandshakeType_t handshake_type, const std::string& content)
	{
		return tls_sendRecord(TlsContentType::handshake, handshaker->pack(handshake_type, content));
	}

	bool Socket::tls_sendRecord(TlsContentType_t content_type, const std::string& content)
	{
		if (!tls_encrypter_send.isActive())
		{
			TlsRecord record{};
			record.content_type = content_type;
			record.length = static_cast<uint16_t>(content.size());
			auto data = record.toBinaryString();
			data.append(content);
			return transport_send(data);
		}

		return tls_sendRecordEncrypted(content_type, content);
	}

	bool Socket::tls_sendRecordEncrypted(TlsContentType_t content_type, const std::string& content)
	{
		auto body = tls_encrypter_send.encrypt(content_type, content);

		TlsRecord record{};
		record.content_type = content_type;
		record.length = static_cast<uint16_t>(body.size());

		Buffer buf(5 + body.size());
		BufferRefWriter bw(buf, BIG_ENDIAN);
		record.write(bw);

		buf.append(body.data(), body.size());

		return transport_send(buf);
	}

	struct CaptureSocketTlsRecvHandshake
	{
		UniquePtr<SocketTlsHandshaker> handshaker;
		void(*callback)(Socket&, UniquePtr<SocketTlsHandshaker>&&, TlsHandshakeType_t, std::string&&);
		std::string pre;
		bool is_new_bytes = false;
	};

	void Socket::tls_recvHandshake(UniquePtr<SocketTlsHandshaker>&& handshaker, void(*callback)(Socket&, UniquePtr<SocketTlsHandshaker>&&, TlsHandshakeType_t, std::string&&), std::string&& pre)
	{
		CaptureSocketTlsRecvHandshake cap{
			std::move(handshaker),
			callback,
			std::move(pre)
		};

		auto record_callback = [](Socket& s, TlsContentType_t content_type, std::string&& data, Capture&& _cap)
		{
			if (content_type != TlsContentType::handshake)
			{
#if LOGGING
				if (content_type == TlsContentType::alert)
				{
					std::string msg = s.toString();
					msg.append(" - Remote closing connection with ");
					if (data.at(0) == 2)
					{
						msg.append("fatal ");
					}
					msg.append("alert, description ");
					msg.append(std::to_string((int)data.at(1)));
					msg.append(". See TlsAlertDescription for details.");
					logWriteLine(std::move(msg));
				}
				else
				{
					std::string msg = "Unexpected content type; expected handshake, found ";
					msg.append(std::to_string((int)content_type));
					logWriteLine(std::move(msg));
				}
#endif
				s.tls_close(TlsAlertDescription::unexpected_message);
				return;
			}

			auto& cap = _cap.get<CaptureSocketTlsRecvHandshake>();

			if (cap.is_new_bytes)
			{
				cap.handshaker->layer_bytes.append(data);
			}

			if (!cap.pre.empty())
			{
				data.insert(0, cap.pre);
			}

			TlsHandshake hs;
			if (!hs.fromBinary(data))
			{
				s.tls_close(TlsAlertDescription::decode_error);
				return;
			}

			if (hs.length > (data.size() - 4))
			{
				s.tls_recvHandshake(std::move(cap.handshaker), cap.callback, std::move(data));
				return;
			}

			data.erase(0, 4);

			if (data.size() > hs.length)
			{
				s.tls_record_buf = data.substr(hs.length);
				data.erase(hs.length);
			}

			cap.callback(s, std::move(cap.handshaker), hs.handshake_type, std::move(data));
		};

		if (!tls_record_buf.empty())
		{
			std::string data = std::move(tls_record_buf);
			tls_record_buf.clear();
			record_callback(*this, TlsContentType::handshake, std::move(data), std::move(cap));
			return;
		}

		cap.is_new_bytes = true;
		tls_recvRecord(record_callback, std::move(cap));
	}

	struct CaptureSocketTlsRecvRecordExpect
	{
		TlsContentType_t expected_content_type;
		void(*callback)(Socket&, std::string&&, Capture&&);
		Capture cap;
	};

	void Socket::tls_recvRecord(TlsContentType_t expected_content_type, void(*callback)(Socket&, std::string&&, Capture&&), Capture&& cap)
	{
		tls_recvRecord([](Socket& s, TlsContentType_t content_type, std::string&& data, Capture&& _cap)
		{
			auto& cap = _cap.get<CaptureSocketTlsRecvRecordExpect>();
			if (content_type == cap.expected_content_type)
			{
				cap.callback(s, std::move(data), std::move(cap.cap));
			}
			else if (content_type == TlsContentType::alert)
			{
#if LOGGING
				{
					std::string msg = s.toString();
					msg.append(" - Remote closing connection with ");
					if (data.at(0) == 2)
					{
						msg.append("fatal ");
					}
					msg.append("alert, description ");
					msg.append(std::to_string((int)data.at(1)));
					msg.append(". See TlsAlertDescription for details.");
					logWriteLine(std::move(msg));
				}
#endif
				s.remote_closed = true;
				s.close();
				if (s.callback_recv_on_close)
				{
					cap.callback(s, {}, std::move(cap.cap));
				}
			}
			else
			{
				s.tls_close(TlsAlertDescription::unexpected_message);
			}
		}, CaptureSocketTlsRecvRecordExpect{
			expected_content_type,
			callback,
			std::move(cap)
		});
	}

	struct CaptureSocketTlsRecvRecord1
	{
		void(*callback)(Socket&, TlsContentType_t, std::string&&, Capture&&);
		Capture cap;
	};

	struct CaptureSocketTlsRecvRecord2
	{
		CaptureSocketTlsRecvRecord1 prev;
		TlsContentType_t content_type;
	};

	void Socket::tls_recvRecord(void(*callback)(Socket&, TlsContentType_t, std::string&&, Capture&&), Capture&& cap)
	{
		if (!tls_record_buf.empty())
		{
			std::string data = std::move(tls_record_buf);
			tls_record_buf.clear();
			callback(*this, TlsContentType::handshake, std::move(data), std::move(cap));
			return;
		}
		transport_recvExact(5, [](Socket& s, std::string&& data, Capture&& cap)
		{
			TlsRecord record{};
			if (!record.fromBinary(data)
				|| record.version.major != 3
				)
			{
				s.tls_close(TlsAlertDescription::decode_error);
				return;
			}
			s.transport_recvExact(record.length, [](Socket& s, std::string&& data, Capture&& _cap)
			{
				auto& cap = _cap.get<CaptureSocketTlsRecvRecord2>();
				if (s.tls_encrypter_recv.isActive())
				{
					constexpr auto cipher_bytes = 16;

					if (!s.tls_encrypter_recv.isAead())
					{
						constexpr auto record_iv_length = 16;
						const auto mac_length = s.tls_encrypter_recv.mac_key.size();

						if ((data.size() % cipher_bytes) != 0
							|| data.size() < (cipher_bytes + mac_length)
							)
						{
							s.tls_close(TlsAlertDescription::bad_record_mac);
							return;
						}

						auto iv = data.substr(0, record_iv_length);
						data.erase(0, record_iv_length);
						aes::cbcDecrypt(
							reinterpret_cast<uint8_t*>(data.data()), data.size(),
							reinterpret_cast<const uint8_t*>(s.tls_encrypter_recv.cipher_key.data()), s.tls_encrypter_recv.cipher_key.size(),
							reinterpret_cast<const uint8_t*>(iv.data())
						);

						std::string mac{};

						bool pad_mismatch = false;
						uint8_t pad_len = data.back();
						for (auto it = (data.end() - (pad_len + 1)); it != (data.end() - 1); ++it)
						{
							if (*it != pad_len)
							{
								pad_mismatch = true;
							}
						}
						if (data.size() >= pad_len)
						{
							data.erase(data.size() - (pad_len + 1));

							if (data.size() > mac_length)
							{
								mac = data.substr(data.size() - mac_length);
								data.erase(data.size() - mac_length);
							}
						}

						if (s.tls_encrypter_recv.calculateMac(cap.content_type, data) != mac
							|| pad_mismatch
							)
						{
							s.tls_close(TlsAlertDescription::bad_record_mac);
							return;
						}
					}
					else
					{
						constexpr auto record_iv_length = 8;

						if (data.size() < (record_iv_length + cipher_bytes))
						{
							s.tls_close(TlsAlertDescription::bad_record_mac);
							return;
						}

						auto tag = data.substr(data.length() - cipher_bytes);

						if (tag.size() != cipher_bytes)
						{
							s.tls_close(TlsAlertDescription::bad_record_mac);
							return;
						}

						data.erase(data.length() - cipher_bytes);

						auto iv = s.tls_encrypter_recv.implicit_iv;
						auto nonce_explicit = data.substr(0, record_iv_length);
						iv.insert(iv.end(), nonce_explicit.begin(), nonce_explicit.end());
						data.erase(0, record_iv_length);

						auto ad = s.tls_encrypter_recv.calculateMacBytes(cap.content_type, data);

						if (aes::gcmDecrypt(
							(uint8_t*)data.data(), data.size(),
							(const uint8_t*)ad.data(), ad.size(),
							s.tls_encrypter_recv.cipher_key.data(), s.tls_encrypter_recv.cipher_key.size(),
							iv.data(), iv.size(),
							(const uint8_t*)tag.data()
						) != true)
						{
							s.tls_close(TlsAlertDescription::bad_record_mac);
							return;
						}
					}
				}
				cap.prev.callback(s, cap.content_type, std::move(data), std::move(cap.prev.cap));
			}, CaptureSocketTlsRecvRecord2{
				std::move(cap.get<CaptureSocketTlsRecvRecord1>()),
				record.content_type
			});
		}, CaptureSocketTlsRecvRecord1{
			callback,
			std::move(cap)
		});
	}

	void Socket::tls_close(TlsAlertDescription_t desc)
	{
		if (hasConnection())
		{
			{
				std::string bin(1, '\2'); // fatal
				bin.push_back((char)desc); static_assert(sizeof(TlsAlertDescription_t) == sizeof(char));
				tls_sendRecord(TlsContentType::alert, bin);
			}

			tls_encrypter_send.reset();
			tls_encrypter_recv.reset();

			transport_close();
		}
	}

	bool Socket::transport_hasData() const
	{
		char buf;
		return ::recv(fd, &buf, 1, MSG_PEEK) == 1;
	}

	bool Socket::transport_send(const Buffer& buf) const noexcept
	{
		return transport_send(buf.data(), static_cast<int>(buf.size()));
	}

	bool Socket::transport_send(const std::string& data) const noexcept
	{
		return transport_send(data.data(), static_cast<int>(data.size()));
	}

	bool Socket::transport_send(const void* data, int size) const noexcept
	{
		return ::send(fd, (const char*)data, size, 0) == size;
	}

	std::string Socket::transport_recvCommon(int max_bytes)
	{
		std::string buf(max_bytes, '\0');
		auto res = ::recv(fd, buf.data(), max_bytes, 0);
		if (res > 0)
		{
			buf.resize(res);
			return buf;
		}
#if SOUP_LINUX
		/*else*/ if (res == 0
			|| (/*res == -1 &&*/
//#if SOUP_WINDOWS
//				WSAGetLastError() != WSAEWOULDBLOCK
//#else
				errno != EWOULDBLOCK && errno != EAGAIN
//#endif
				)
			)
		{
			close();
		}
#endif
		return {};
	}

	struct CaptureSocketTransportRecv
	{
		int bytes;
		Socket::transport_recv_callback_t callback;
		Capture cap;
	};

	void Socket::transport_recv(int max_bytes, transport_recv_callback_t callback, Capture&& cap)
	{
		if (canRecurse())
		{
			// In the case of remote_closed, recv callback would only be called if there's still data available or the user set callback_recv_on_close.
			if (auto buf = transport_recvCommon(max_bytes); !buf.empty() || remote_closed)
			{
				callback(*this, std::move(buf), std::move(cap));
				return;
			}
		}
		holdup_type = SOCKET;
		holdup_callback.set([](Worker& w, Capture&& _cap)
		{
			w.holdup_type = Worker::NONE;
			auto& cap = _cap.get<CaptureSocketTransportRecv>();
			static_cast<Socket&>(w).transport_recv(cap.bytes, cap.callback, std::move(cap.cap));
		}, CaptureSocketTransportRecv{ max_bytes, callback, std::move(cap) });
	}

	struct CaptureSocketTransportRecvExact : public CaptureSocketTransportRecv
	{
		std::string buf;

		CaptureSocketTransportRecvExact(int bytes, Socket::transport_recv_callback_t callback, Capture&& cap, std::string&& buf)
			: CaptureSocketTransportRecv{ bytes, callback, std::move(cap) }, buf(std::move(buf))
		{
		}
	};

	void Socket::transport_recvExact(int bytes, transport_recv_callback_t callback, Capture&& cap, std::string&& pre)
	{
		if (canRecurse())
		{
			auto remainder = static_cast<int>(bytes - pre.size());
			if (auto buf = transport_recvCommon(remainder); !buf.empty())
			{
				pre.append(buf);
			}
			if (pre.size() == bytes)
			{
				callback(*this, std::move(pre), std::move(cap));
				return;
			}
			if (remote_closed)
			{
				return;
			}
		}
		holdup_type = SOCKET;
		holdup_callback.set([](Worker& w, Capture&& _cap)
		{
			w.holdup_type = Worker::NONE;
			auto& cap = _cap.get<CaptureSocketTransportRecvExact>();
			static_cast<Socket&>(w).transport_recvExact(cap.bytes, cap.callback, std::move(cap.cap), std::move(cap.buf));
		}, CaptureSocketTransportRecvExact(bytes, callback, std::move(cap), std::move(pre)));
	}

	void Socket::transport_close() noexcept
	{
		if (hasConnection())
		{
#if SOUP_WINDOWS
			::closesocket(fd);
#else
			::close(fd);
#endif
			fd = -1;
		}
	}

	bool Socket::isWorkDoneOrClosed() const noexcept
	{
		return isWorkDone()
			|| !hasConnection()
			|| remote_closed
			;
	}

	void Socket::keepAlive()
	{
		recv([](Socket&, std::string&&, Capture&&)
		{
			// If we actually receive something in this state, we just let the scheduler delete the socket since it won't have a holdup anymore.
		});
	}

	std::string Socket::toString() const
	{
		return peer.toString();
	}
}

#endif
