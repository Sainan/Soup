#include "netIntel.hpp"

#include <sstream>

#include "csv.hpp"
#include "deflate.hpp"
#include "string.hpp"
#include "WebResource.hpp"

namespace soup
{
	void netIntel::init(bool ipv4, bool ipv6)
	{
		initAs(ipv4, ipv6);
		initLocation(ipv4, ipv6);
	}

	void netIntel::initAs(bool ipv4, bool ipv6)
	{
		initAsList();
		if (ipv4)
		{
			initIpv4ToAs();
		}
		if (ipv6)
		{
			initIpv6ToAs();
		}
	}

	void netIntel::initLocation(bool ipv4, bool ipv6)
	{
		if (ipv4)
		{
			initIpv4ToLocation();
		}
		if (ipv6)
		{
			initIpv6ToLocation();
		}
	}

	void netIntel::initAsList()
	{
		std::stringstream aslistcsv{};
		{
			WebResource rsc("github.com", "/Umkus/asn-ip/releases/download/latest/as.csv");
			rsc.downloadWithCaching();
			aslistcsv << std::move(rsc.data);
		}
		std::string line;
		std::getline(aslistcsv, line); // skip field names
		while (std::getline(aslistcsv, line))
		{
			auto asn_sep = line.find(',');
			SOUP_IF_UNLIKELY(asn_sep == std::string::npos)
			{
				continue;
			}
			netAs as;
			as.number = string::toInt<uint32_t>(line.substr(0, asn_sep)).value();
			++asn_sep;
			auto handle_sep = line.find(',', asn_sep);
			as.handle = as_pool.emplace(line.substr(asn_sep, handle_sep - asn_sep));
			as.name = as_pool.emplace(line.substr(handle_sep + 2, line.length() - handle_sep - 3));
			aslist.emplace(as.number, soup::make_unique<netAs>(std::move(as)));
		}
	}

	void netIntel::initIpv4ToAs()
	{
		std::stringstream ipv4toasntsv{};
		{
			WebResource rsc("iptoasn.com", "/data/ip2asn-v4-u32.tsv.gz");
			rsc.downloadWithCaching();
			ipv4toasntsv << deflate::decompress(std::move(rsc.data)).decompressed;
		}
		for (std::string line; std::getline(ipv4toasntsv, line); )
		{
			auto arr = string::explode(line, '\t');
			SOUP_IF_UNLIKELY(arr.size() < 5)
			{
				continue;
			}
			uint32_t asn = string::toInt<uint32_t>(arr.at(2)).value();
			if (asn == 0)
			{
				continue;
			}
			auto begin = string::toInt<uint32_t>(arr.at(0)).value();
			auto end = string::toInt<uint32_t>(arr.at(1)).value();
			netAs* as = getAsByNumber(asn);
			if (as == nullptr)
			{
				as = aslist.emplace(asn, soup::make_unique<netAs>(asn, as_pool.emplace(std::move(arr.at(4))))).first->second.get();
			}
			ipv4toas.emplace(begin, end, as);
		}
	}

	void netIntel::initIpv6ToAs()
	{
		std::stringstream ipv6toasntsv{};
		{
			WebResource rsc("iptoasn.com", "/data/ip2asn-v6.tsv.gz");
			rsc.downloadWithCaching();
			ipv6toasntsv << deflate::decompress(std::move(rsc.data)).decompressed;
		}
		for (std::string line; std::getline(ipv6toasntsv, line); )
		{
			auto arr = string::explode(line, '\t');
			SOUP_IF_UNLIKELY(arr.size() < 5)
			{
				continue;
			}
			uint32_t asn = string::toInt<uint32_t>(arr.at(2)).value();
			if (asn == 0)
			{
				continue;
			}
			IpAddr begin = arr.at(0);
			IpAddr end = arr.at(1);
			netAs* as = getAsByNumber(asn);
			if (as == nullptr)
			{
				as = aslist.emplace(asn, soup::make_unique<netAs>(asn, as_pool.emplace(std::move(arr.at(4))))).first->second.get();
			}
			ipv6toas.emplace(std::move(begin), std::move(end), as);
		}
	}

	void netIntel::initIpv4ToLocation()
	{
		std::stringstream ipv4tolocationcsv{};
		{
			WebResource rsc("github.com", "/sapics/ip-location-db/raw/master/dbip-city/dbip-city-ipv4-num.csv.gz");
			rsc.downloadWithCaching();
			ipv4tolocationcsv << deflate::decompress(std::move(rsc.data)).decompressed;
		}
		ipv4tolocation.reserve(3000000);
		for (std::string line; std::getline(ipv4tolocationcsv, line); )
		{
			auto arr = csv::parseLine(line);
			SOUP_IF_UNLIKELY(arr.size() < 6)
			{
				continue;
			}
			ipv4tolocation.emplace(
				string::toInt<uint32_t>(arr.at(0)).value(),
				string::toInt<uint32_t>(arr.at(1)).value(),
				netIntelLocationData{
					std::move(arr.at(2)),
					location_pool.emplace(std::move(arr.at(3))),
					location_pool.emplace(std::move(arr.at(5))),
				}
			);
		}
	}

	void netIntel::initIpv6ToLocation()
	{
		std::stringstream ipv6tolocationcsv{};
		{
			WebResource rsc("github.com", "/sapics/ip-location-db/raw/master/dbip-city/dbip-city-ipv6.csv.gz");
			rsc.downloadWithCaching();
			ipv6tolocationcsv << deflate::decompress(std::move(rsc.data)).decompressed;
		}
		ipv6tolocation.reserve(3000000);
		for (std::string line; std::getline(ipv6tolocationcsv, line); )
		{
			auto arr = csv::parseLine(line);
			SOUP_IF_UNLIKELY(arr.size() < 6)
			{
				continue;
			}
			ipv6tolocation.emplace(arr.at(0), arr.at(1), netIntelLocationData{
				std::move(arr.at(2)),
				location_pool.emplace(std::move(arr.at(3))),
				location_pool.emplace(std::move(arr.at(5))),
			});
		}
	}

	netAs* netIntel::getAsByNumber(uint32_t number) noexcept
	{
		if (auto e = aslist.find(number); e != aslist.end())
		{
			return e->second.get();
		}
		return nullptr;
	}

	netAs* netIntel::getAsByIp(const IpAddr& addr)
	{
		return addr.isV4()
			? getAsByIpv4(addr.getV4NativeEndian())
			: getAsByIpv6(addr)
			;
	}

	netAs* netIntel::getAsByIpv4(uint32_t ip)
	{
		auto e = ipv4toas.find(ip);
		if (e == nullptr)
		{
			return nullptr;
		}
		return *e;
	}

	netAs* netIntel::getAsByIpv6(const IpAddr& addr)
	{
		auto e = ipv6toas.find(addr);
		if (e == nullptr)
		{
			return nullptr;
		}
		return *e;
	}

	netIntelLocationData* netIntel::getLocationByIp(const IpAddr& addr)
	{
		return addr.isV4()
			? getLocationByIpv4(addr.getV4NativeEndian())
			: getLocationByIpv6(addr)
			;
	}
	
	netIntelLocationData* netIntel::getLocationByIpv4(uint32_t ip)
	{
		return ipv4tolocation.find(ip);
	}

	netIntelLocationData* netIntel::getLocationByIpv6(const IpAddr& addr)
	{
		return ipv6tolocation.find(addr);
	}
}
