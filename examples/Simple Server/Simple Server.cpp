#include <iostream>
#include <thread>

#include <asn1_sequence.hpp>
#include <pem.hpp>
#include <rsa.hpp>
#include <scheduler.hpp>
#include <server.hpp>
#include <socket.hpp>
#include <socket_tls_server_rsa_data.hpp>

static void sendHtml(soup::socket& s, std::string body)
{
	auto len = body.size();
	body.insert(0, "\r\n\r\n");
	body.insert(0, std::to_string(len));
	body.insert(0, "HTTP/1.0 200\r\nContent-Length: ");
	s.send(body);
	s.close();
}

static void httpRecv(soup::socket& s)
{
	s.recv([](soup::socket& s, std::string&& data, soup::capture&&)
	{
		auto i = data.find(' ');
		if (i == std::string::npos)
		{
			s.send("HTTP/1.0 400\r\n\r\n");
			s.close();
			return;
		}
		data.erase(0, i + 1);
		i = data.find(' ');
		if (i == std::string::npos)
		{
			s.send("HTTP/1.0 400\r\n\r\n");
			s.close();
			return;
		}
		data.erase(i);
		std::cout << s.peer.toString() << " > " << data << std::endl;
		if (data == "/")
		{
			sendHtml(s, R"EOC(<html>
<head>
	<title>Soup</title>
</head>
<body>
	<h1>Soup</h1>
	<p>Soup is a C++ framework that is currently private.</p>
	<p>The website you are currently viewing is directly delivered to you via a relatively simple server, using Soup's powerful abstractions.</p>
	<ul>
		<li><a href="pem-decoder">PEM Decoder</a> - Using Soup's JS API, powered by WASM.</li>
	</ul>
</body>
</html>
)EOC");
		}
		else if (data == "/pem-decoder")
		{
			sendHtml(s, R"EOC(<html>
<head>
	<title>PEM Decoder | Soup</title>
</head>
<body>
	<h1>PEM Decoder</h1>
	<textarea oninput="processInput(event)"></textarea>
	<pre></pre>
	<script src="https://use.soup.do"></script>
	<script>
		function processInput(e)
		{
			if(soup.ready)
			{
				var seq = soup.asn1_sequence.new(soup.pem.decode(e.target.value));
				document.querySelector("pre").textContent = soup.asn1_sequence.toString(seq);
				soup.asn1_sequence.free(seq);
			}
		}
	</script>
</body>
</html>
)EOC");
		}
		else
		{
			s.send("HTTP/1.0 404\r\n\r\n");
			s.close();
		}
	});
}

static void httpRecv(soup::socket& s, soup::capture&&)
{
	return httpRecv(s);
}

static soup::socket_tls_server_rsa_data server_rsa_data;

int main()
{
	soup::server srv{};
	if (!srv.bind(80))
	{
		std::cout << "Failed to bind to port 80." << std::endl;
#if SOUP_LINUX
		std::cout << "Run { fuser 80/tcp } and try again." << std::endl;
#endif
		return 1;
	}
	if (!srv.bind(443))
	{
		std::cout << "Failed to bind to port 443." << std::endl;
#if SOUP_LINUX
		std::cout << "Run { fuser 443/tcp } and try again." << std::endl;
#endif
		return 2;
	}
	server_rsa_data.der_encoded_certchain = {
		soup::pem::decode(R"EOC(
-----BEGIN CERTIFICATE-----
MIIFEjCCA/qgAwIBAgISAzO1ak1tzkSo99OqAn2x+OfMMA0GCSqGSIb3DQEBCwUA
MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD
EwJSMzAeFw0yMjAzMDcwMjM0MzdaFw0yMjA2MDUwMjM0MzZaMBIxEDAOBgNVBAMT
B3NvdXAuZG8wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDYbeJly69N
d+bPEnzCEAJZzqp/xsGr2YTgisYGyBKqp0ubWfl4ItRx7a50siEVy57oNBc4AGhQ
6/A1+IFQ2n/kGvvY0sTLs4Pv1yRD1Qs2VkDS6Jcor5O+tzvOomHko7mJ5cvxIq15
rocdKsJGhFY7k3S+iY0pXk5niEDMts6JMrXOja9oDt5pWoDNDECfIN7D1THB+v1i
8tsS5ScFCgVpFf9fLW8sW0weeEp0mpAIO5LUdQWWRFd7csMtVorJauYy5b0qKKZV
6eUWy04OrhZL+Adqg1KT/LBvO69oZNBoCOehoVJnoqgMTfTweUTPpNGYmlNanG7J
Qu++GqrcfzRzAgMBAAGjggJAMIICPDAOBgNVHQ8BAf8EBAMCBaAwHQYDVR0lBBYw
FAYIKwYBBQUHAwEGCCsGAQUFBwMCMAwGA1UdEwEB/wQCMAAwHQYDVR0OBBYEFE7Q
uqEvBJXbxPcOKhxv6fZHEMchMB8GA1UdIwQYMBaAFBQusxe3WFbLrlAJQOYfr52L
FMLGMFUGCCsGAQUFBwEBBEkwRzAhBggrBgEFBQcwAYYVaHR0cDovL3IzLm8ubGVu
Y3Iub3JnMCIGCCsGAQUFBzAChhZodHRwOi8vcjMuaS5sZW5jci5vcmcvMBIGA1Ud
EQQLMAmCB3NvdXAuZG8wTAYDVR0gBEUwQzAIBgZngQwBAgEwNwYLKwYBBAGC3xMB
AQEwKDAmBggrBgEFBQcCARYaaHR0cDovL2Nwcy5sZXRzZW5jcnlwdC5vcmcwggEC
BgorBgEEAdZ5AgQCBIHzBIHwAO4AdQDfpV6raIJPH2yt7rhfTj5a6s2iEqRqXo47
EsAgRFwqcwAAAX9icXU+AAAEAwBGMEQCIGn3bY2k5A2Bm6Vj/MJzsu37VR7VgCK9
DGlZE1uIiJmlAiB03bP3Yzi2IMq7kZ7iTyN73jX5BjN/1nSdG10jHOP35gB1ACl5
vvCeOTkh8FZzn2Old+W+V32cYAr4+U1dJlwlXceEAAABf2JxdTAAAAQDAEYwRAIg
UpJu1XNFarHUfzbjAhb0+dZA9VlB/soZ51BJq/bIXkYCIFqCNaaLy8mXfS9E+pGs
wnHTvAAzIVZhbZc/+rzH+JY0MA0GCSqGSIb3DQEBCwUAA4IBAQBSfR9b4Wt17a8l
g1PMh0d2m7B5WMYuovOwd1A8KEOavYe2QqIQ1olS1vPKPFwtp1dk4qhqWd3vCCGf
84cCfwpnwrk1bSeT4PBy3z1hOHaZ+iFH/GXRC6B18MWueRMmM9k4McKUTjQzRatm
Am+yUiq0BvgST2Ctf/LZbKsecvTnkLPzUededMfO3s631JIFu/8Uh0gvE+daucdV
CFz9GEwFTtBOZVm2pXQa7WJZkkas2CEp8OaE1uTi0dNsjsmoucx5NjnAVIJsZYjl
fSAiz4aov7Yb7NyFQqfMAhjYQIxkoJSHwhTWRTlccEiwtPvOU+e3lYQVemWlHw7W
eG23qKPg
-----END CERTIFICATE-----
)EOC")
	};
	server_rsa_data.private_key = soup::rsa::key_private::fromAsn1(soup::asn1_sequence::fromBinary(soup::pem::decode(R"EOC(
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDYbeJly69Nd+bP
EnzCEAJZzqp/xsGr2YTgisYGyBKqp0ubWfl4ItRx7a50siEVy57oNBc4AGhQ6/A1
+IFQ2n/kGvvY0sTLs4Pv1yRD1Qs2VkDS6Jcor5O+tzvOomHko7mJ5cvxIq15rocd
KsJGhFY7k3S+iY0pXk5niEDMts6JMrXOja9oDt5pWoDNDECfIN7D1THB+v1i8tsS
5ScFCgVpFf9fLW8sW0weeEp0mpAIO5LUdQWWRFd7csMtVorJauYy5b0qKKZV6eUW
y04OrhZL+Adqg1KT/LBvO69oZNBoCOehoVJnoqgMTfTweUTPpNGYmlNanG7JQu++
GqrcfzRzAgMBAAECggEBAKGQLfClw6CGAFPWTjGkN80I3PhzzAHYaDwi/D71vhGM
v4EiAnvvLD48Gv5cNxyJG3/l2utgSn8WEgSIFSjhY5VJm3W5qVUTFkvFg/nrIOqY
Kt4G6UhjAVzedhQD3iYLHqdVVxAUPgHXCl/4mnx/r8vbgMv37NvT3Z2l9hGb6cQ6
KThWdtse2/cG7bwnQQWQI/5gd/3technnzYtPh47s2Z0fj0CUp8oA9eZa2oZhmFd
5wNnX5Sy5OSZQlOJHEg/9kN9GkahtyjTQ6IGiocuU0Fb788ZF0QEZuB2ddc87lAB
hiAd1TgbieU+11idqh1C5wCLCPtAevXRhQNhdKeubSECgYEA/uOtO0jmRT6jFf1A
XtUPaJ6kWyjSA6fu/HoAsGYzxz1yiUDM0PWyXWQ8dUYe41pd6QDu++l4fkJLB6LC
Ks7hewsCqZIRw8w3ebbABqWBFW4RT3w+nYOdaJwrHUbwg+YoLrC6iJwOusC/FKr5
p6USYp38O5RE2Xb12j9F8dcJl2MCgYEA2V9OaFb+PTKCL6PHZYoeTg4ElVLHEq5E
H77dJYAd2A75amEKC3L5WqYiLTrOdBqIOhXXSUSMNd2pEecw6u50A5vI0ezJP40/
bEYQDwpQDhdtvhWPtLJ1C0GsCyjgHwpmcEAD3rNB6M0nIROD/BKIaABePD7KoOu8
yEbdWJeZI7ECgYBEaWt3fAuCDlvLbRu32Eu4csv+Q6iKnqpATaadsfC3y0BQonnW
o/tpoZuwhk+IChsmjL+YEYPrr3Nf60leIATY942RYcku2kMRggFsR0OsMsymntxX
fpnjF/didkXbwQyL65dFT02MxmsC6xjy7BVRLsIiY5tPGuTF3TGyxVqnrQKBgQC6
82IvEOq2XXNkX7rFlMW9ogbFGp2GboS+vNvcPdTtFuviVzVZZXgaQ5pPRi1747nY
IyK2rBLe3RZlBG6pD46N7/UGv1zSoLu0domnNdpmVDYZbtfatEU/+ipqqqwfZkV2
M0hgx9Fe1NrbcrpoGNRihjaGIAcL4dPKeFA0uqWF8QKBgFN4K7Veh7E8nJ8D/ErB
AA3w0djptVzPMBF7JFE72qgIYiErviEf2X5aFwTLQJNtpLBNDFGhlsyM0B3vQMG6
IWTRPUZRNojVvK1dQ+xPN/9HsFVUb6JWyU4e3gocnYoe2zGdyT9p9u0Pr3JikgAC
QJg24g1I/Zb4EUJmo2WNBzGS
-----END PRIVATE KEY-----
)EOC")));
	std::cout << "Listening on ports 80 and 443." << std::endl;
	srv.on_accept = [](soup::socket& s, uint16_t port)
	{
		std::cout << s.peer.toString()  << " + connected at port " << port << std::endl;
		if (port == 80)
		{
			httpRecv(s);
		}
		else
		{
			s.enableCryptoServer([](soup::socket_tls_server_rsa_data& out, const std::string& server_name)
			{
				out = server_rsa_data;
			}, &httpRecv);
		}
	};
	srv.on_work_done = [](soup::worker& w)
	{
		std::cout << reinterpret_cast<soup::socket&>(w).peer.toString() << " - work done" << std::endl;
	};
	srv.on_connection_lost = [](soup::socket& s)
	{
		std::cout << s.peer.toString() << " - connection lost" << std::endl;
	};
	srv.on_exception = [](soup::worker& w, const std::exception& e)
	{
		std::cout << reinterpret_cast<soup::socket&>(w).peer.toString() << " - exception: " << e.what() << std::endl;
	};
	srv.run();
}
