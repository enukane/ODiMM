//=============================================================================
// Brief   : Tunnel Service Test Application
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

#include <opmip/sys/tunnel_service.hpp>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
int main()
{
	using namespace opmip;

	const char* const name = "pmip0";
	boost::system::error_code ec;
	sys::ip6_tunnel_service::parameters tpm0;
	sys::ip6_tunnel_service::parameters tpm1;
	sys::ip6_tunnel_service::parameters tpm2;
	sys::ip6_tunnel_service::parameters tpm3;
	boost::asio::io_service ios;
	sys::ip6_tunnel_service ts(ios);

	try {
		tpm0.name(name);
		tpm0.local_address(sys::ip::address_v6::from_string("2001:c188:5d6e:7:218:f3ff:fe90:6d6f"));
		tpm0.flags(sys::ip6_tunnel_service::parameters::ignore_encapsulation_limit
		           | sys::ip6_tunnel_service::parameters::use_original_traffic_class);
		ts.add(tpm0, ec);
		std::cout << "tpm0: " << tpm0 << std::endl;

		tpm1.name(name);
		ts.get(tpm1, ec);
		std::cout << "tpm1: " << tpm1 << std::endl;

		tpm2.name(name);
		tpm2.device(2);
		tpm2.local_address(sys::ip::address_v6::from_string("2002:c188:5d6e:7:218:f3ff:fe90:6d6f"));
		ts.change(tpm2, ec);
		std::cout << "tpm2: " << tpm2 << std::endl;

		tpm3.name(name);
		ts.remove(tpm3, ec);
		std::cout << "tpm3: " << tpm3 << std::endl;

	} catch (...) {
		boost::system::error_code ec;
		tpm3.name(name);
		ts.remove(tpm3, ec);

		throw;
	}
}

// EOF ////////////////////////////////////////////////////////////////////////