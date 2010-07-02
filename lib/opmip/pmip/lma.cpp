//=============================================================================
// Brief   : Local Mobility Anchor
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

#include <opmip/pmip/lma.hpp>
#include <opmip/pmip/mp_sender.hpp>
#include <opmip/exception.hpp>
#include <boost/bind.hpp>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
namespace opmip { namespace pmip {

///////////////////////////////////////////////////////////////////////////////
bool validate_sequence_number(uint16 prev, uint16 current)
{
	return ((current > prev) && (current < (prev + 32768)));
}

///////////////////////////////////////////////////////////////////////////////
lma::lma(boost::asio::io_service& ios, node_db& ndb, size_t concurrency)
	: _service(ios), _node_db(ndb), _log("LMA", &std::cout), _mp_sock(ios),
	  _route_table(ios), _tunnel_dev(0), _home_net_dev(0),
	  _concurrency(concurrency), _tunnel(ios)
{
}

void lma::start(const char* id, const ip_address& home_network_link)
{
	_service.dispatch(boost::bind(&lma::istart, this, id, home_network_link));
}

void lma::stop()
{
	_service.dispatch(boost::bind(&lma::istop, this));
}

void lma::mp_send_handler(const boost::system::error_code& ec)
{
	if (ec && ec != boost::system::errc::make_error_condition(boost::system::errc::operation_canceled))
		_log(0, "PBA sender error: ", ec.message());
}

void lma::mp_receive_handler(const boost::system::error_code& ec, const proxy_binding_info& pbinfo, pbu_receiver_ptr& pbur)
{
	if (ec) {
		if (ec != boost::system::errc::make_error_condition(boost::system::errc::operation_canceled))
			_log(0, "PBU receiver error: ", ec.message());
		return;
	}

	_service.dispatch(boost::bind(&lma::iproxy_binding_update, this, pbinfo));
	pbur->async_receive(_mp_sock, boost::bind(&lma::mp_receive_handler, this, _1, _2, _3));
}

void lma::bcache_remove_entry(const boost::system::error_code& ec, const std::string& mn_id)
{
	if (ec) {
		if (ec != boost::system::errc::make_error_condition(boost::system::errc::operation_canceled))
			_log(0, "Binding cache remove entry timer error: ", ec.message());
		return;
	}

	_service.dispatch(boost::bind(&lma::ibcache_remove_entry, this, mn_id));
}

void lma::istart(const char* id, const ip_address& home_network_link)
{
	_log(0, "Starting... [id = ", id, ", home_network_link = ", home_network_link, "]");

	if (!home_network_link.is_link_local()) {
		error_code ec(boost::system::errc::invalid_argument, boost::system::get_generic_category());

		throw_exception(exception(ec, "Home network link must be a link local IPv6 address"));
	}

	const lma_node* node = _node_db.find_lma(id);
	if (!node) {
		error_code ec(boost::system::errc::invalid_argument, boost::system::get_generic_category());

		throw_exception(exception(ec, "LMA id not found in node database"));
	}

	_mp_sock.open(ip::mproto());
	_mp_sock.bind(ip::mproto::endpoint(node->address()));

	_home_net_dev = home_network_link.scope_id();
	_identifier   = id;

	_tunnel.open("lma0", node->device_id(), node->address(), ip_address::any());
	_tunnel.set_enable(true);
	_tunnel_dev = _tunnel.get_device_id();

	for (size_t i = 0; i < _concurrency; ++i) {
		refcount_ptr<pbu_receiver> pbur(new pbu_receiver());

		pbur->async_receive(_mp_sock, boost::bind(&lma::mp_receive_handler, this, _1, _2, _3));
	}
}

void lma::istop()
{
	_log(0, "Stoping...");

	_bcache.clear();
	_tunnel.close();
	_mp_sock.close();
}

void lma::iproxy_binding_update(proxy_binding_info& pbinfo)
{
	_log(0, "PBU [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");

	bcache_entry* be = _bcache.find(pbinfo.id);
	if (!be) {
		if (!pbinfo.lifetime) {
			_log(0, "PBU de-registration error: binding cache entry not found [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");
			return; //error
		}

		const mobile_node* mn = _node_db.find_mobile_node(pbinfo.id);
		if (!mn) {
			_log(0, "PBU registration error: unknown mobile node [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");
			return; //error

		}
		if (mn->lma_id() != _identifier) {
			_log(0, "PBU registration error: not this LMA [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");
			return; //error
		}

		be = new bcache_entry(_service.get_io_service(), pbinfo.id, mn->prefix_list());
		_bcache.insert(be);
	}

	if (be->care_of_address != pbinfo.address) {
		const mag_node* mag = _node_db.find_mag(pbinfo.address);
		if (!mag) {
			_log(0, "PBU error: unknown MAG [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");
			return; //error
		}

		if (!pbinfo.lifetime) {
			_log(0, "PBU de-registration error: not this MAG [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");
			return; //error
		}
		_log(0, "PBU new registration [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");

		if (be->care_of_address.is_unspecified())
			del_route_entries(be);
		be->care_of_address = pbinfo.address;
		be->lifetime = pbinfo.lifetime;
		be->sequence = pbinfo.sequence;
		be->link_type = pbinfo.link_type;

	} else {
		if (!validate_sequence_number(be->sequence, pbinfo.sequence)) {
			_log(0, "PBU error: sequence not valid [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");
			return; //error
		}

		be->lifetime = pbinfo.lifetime;
		be->sequence = pbinfo.sequence;
	}

	if (pbinfo.lifetime && be->bind_status != bcache_entry::k_bind_registered) {
		_log(0, "PBU registration [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");

		be->timer.cancel();
		be->bind_status = bcache_entry::k_bind_registered;
		add_route_entries(be);
	}

	BOOST_ASSERT((be->bind_status != bcache_entry::k_bind_unknown));

	if (!pbinfo.lifetime && be->bind_status == bcache_entry::k_bind_registered) {
		_log(0, "PBU de-registration [id = ", pbinfo.id, ", mag = ", pbinfo.address, "]");

		be->timer.cancel();
		be->bind_status = bcache_entry::k_bind_deregistered;
		del_route_entries(be);

		be->timer.expires_from_now(boost::posix_time::milliseconds(_config.min_delay_before_BCE_delete));
		be->timer.async_wait(boost::bind(&lma::bcache_remove_entry, this, _1, be->id()));
	}

	pbinfo.status = ip::mproto::pba::status_ok;
	pba_sender_ptr pbas(new pba_sender(pbinfo));

	pbas->async_send(_mp_sock, boost::bind(&lma::mp_send_handler, this, _1));
}

void lma::ibcache_remove_entry(const std::string& mn_id)
{
	bcache_entry* be = _bcache.find(mn_id);
	if (!be || be->bind_status != bcache_entry::k_bind_deregistered) {
		_log(0, "Binding cahe remove entry error: not found [id = ", mn_id, "]");
		return;
	}

	_bcache.remove(be);
}

void lma::add_route_entries(bcache_entry* be)
{
	const bcache::net_prefix_list& npl = be->prefix_list();

	_log(0, "Add route entries [id = ", be->id(), "]");

	for (bcache::net_prefix_list::const_iterator i = npl.begin(), e = npl.end(); i != e; ++i)
		_route_table.add_by_src(*i, _tunnel_dev, be->care_of_address);

	for (bcache::net_prefix_list::const_iterator i = npl.begin(), e = npl.end(); i != e; ++i)
		_route_table.add_by_dst(*i, _home_net_dev);
}

void lma::del_route_entries(bcache_entry* be)
{
	const bcache::net_prefix_list& npl = be->prefix_list();

	_log(0, "Remove route entries [id = ", be->id(), "]");

	for (bcache::net_prefix_list::const_iterator i = npl.begin(), e = npl.end(); i != e; ++i)
		_route_table.remove_by_src(*i);

	for (bcache::net_prefix_list::const_iterator i = npl.begin(), e = npl.end(); i != e; ++i)
		_route_table.remove_by_dst(*i);
}

///////////////////////////////////////////////////////////////////////////////
} /* namespace pmip */ } /* namespace opmip */

// EOF ////////////////////////////////////////////////////////////////////////
