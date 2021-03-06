//=============================================================================
// Brief   : ICMPv6 Packet
// Authors : Bruno Santos <bsantos@av.it.pt>
// ----------------------------------------------------------------------------
// OPMIP - Open Proxy Mobile IP
//
// Copyright (C) 2010 Universidade de Aveiro
// Copyrigth (C) 2010 Instituto de Telecomunicações - Pólo de Aveiro
//
// This software is distributed under a license. The full license
// agreement can be found in the file LICENSE in this distribution.
// This software may not be copied, modified, sold or distributed
// other than expressed in the named license agreement.
//
// This software is distributed without any warranty.
//=============================================================================

#ifndef OPMIP_NET_IP_ICMP6_PACKET__HPP_
#define OPMIP_NET_IP_ICMP6_PACKET__HPP_

///////////////////////////////////////////////////////////////////////////////
#include <opmip/base.hpp>
#include <opmip/ip/icmp.hpp>
#include <opmip/ip/checksum.hpp>
#include <opmip/net/ip/ipv6_packet.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace opmip { namespace net { namespace ip {

///////////////////////////////////////////////////////////////////////////////
class icmp6_ra_packet {
public:
	typedef opmip::ip::icmp::router_advertisement     header;
	typedef std::vector<boost::asio::const_buffers_1> const_buffers;

	static const uint8 ipv6_next_header = 58;

public:
	icmp6_ra_packet()
	{ }

	icmp6_ra_packet(uint8  current_hop_limit,
	                uint16 lifetime,
	                uint32 reachable_time,
	                uint32 retrans_timer)
	{
		operator()(current_hop_limit, lifetime, reachable_time, retrans_timer);
	}

	void operator()(uint8  current_hop_limit,
	                uint16 lifetime,
	                uint32 reachable_time,
	                uint32 retrans_timer)
	{
		_header.current_hop_limit(current_hop_limit);
		_header.lifetime(lifetime);
		_header.reachable_time(reachable_time);
		_header.retrans_timer(retrans_timer);
	}

	template<class Option>
	Option* add_option()
	{
		size_t pos = _options.size();

		_options.resize(sizeof(Option) + pos);

		return new(&_options[pos]) Option;
	}

	template<class Option>
	Option* add_option(size_t len)
	{
		size_t pos = _options.size();

		_options.resize(sizeof(Option) + len + pos);

		return new(&_options[pos]) Option(len);
	}

	void notify(const ipv6_packet::pseudo_header& ph)
	{
		_header.checksum(0);
		_checksum.clear();
		_checksum.update(&ph, sizeof(ph));
		_checksum.update(&_header, sizeof(_header));
		_checksum.update(&_options[0], _options.size());
		_header.checksum(_checksum.final());
	}

	size_t size() const { return sizeof(_header) + _options.size(); }

	const_buffers cbuffer() const
	{
		const_buffers buffs;

		buffs.push_back(boost::asio::buffer(&_header, sizeof(header)));
		if (!_options.empty())
			buffs.push_back(boost::asio::buffer(_options));

		return buffs;
	}

private:
	header              _header;
	std::vector<uint8>  _options;
	opmip::ip::checksum _checksum;
};

///////////////////////////////////////////////////////////////////////////////
} /* namespace ip */ } /* namespace net */ } /* namespace opmip */

// EOF ////////////////////////////////////////////////////////////////////////
#endif /* OPMIP_NET_IP_ICMP6_PACKET__HPP_ */
