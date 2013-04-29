//===========================================================================================================
// Brief   : ODiMM - Open Distributed Mobility Management
// Authors : Bruno Santos <bsantos@av.it.pt>
// ----------------------------------------------------------------------------------------------------------
// OPMIP - Open Proxy Mobile IP
//
// Copyright (C) 2013 Universidade de Aveiro
// Copyrigth (C) 2013 Instituto de Telecomunicações - Pólo de Aveiro
//
// This software is distributed under a license. The full license
// agreement can be found in the file LICENSE in this distribution.
// This software may not be copied, modified, sold or distributed
// other than expressed in the named license agreement.
//
// This software is distributed without any warranty.
//===========================================================================================================

#include <odimm/base.hpp>
#include <odimm/debug.hpp>
#include <odimm/logger.hpp>
#include <odimm/exception.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>

#include <odimm/pmip/cmd.hpp>
#include <odimm/pmip/node_db.hpp>
#include "options.hpp"
#include <iostream>
#include <fstream>
#include <cstring>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
static opmip::logger log_("odimm-cmd", std::cout);
///////////////////////////////////////////////////////////////////////////////
static void load_node_database(const std::string& file_name, opmip::pmip::node_db& ndb)
{
	std::ifstream in(file_name.c_str());

	if (!in)
		opmip::throw_exception(opmip::errc::make_error_code(opmip::errc::no_such_file_or_directory),
		                       "Failed to open \"" + file_name + "\" node database file");

	std::pair<size_t, size_t> n = ndb.load(in);
	log_(0, "loaded ", n.first, " router nodes and ", n.second, " mobile nodes from database");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void signal_handler(const boost::system::error_code& error, opmip::pmip::cmd& cmd)
{
	log_(0, "quiting");
	cmd.stop();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	opmip::setup_crash_handler();

	try {
		opmip::app::cmdline_options opts;
		if (!opts.parse(argc, argv))
			return 1;

		size_t                       concurrency = boost::thread::hardware_concurrency();
		boost::asio::io_service      ios(concurrency);
		boost::asio::signal_set      sigs(ios, SIGINT, SIGTERM);
		opmip::pmip::node_db         ndb;
		opmip::pmip::cmd	         cmd(ios, ndb, concurrency);
		
		log_(0, "chrono resolution ", opmip::chrono::get_resolution());

		load_node_database(opts.node_db, ndb);

		cmd.start(opts.identifier.c_str(), opts.tunnel_global_address);

		sigs.async_wait(boost::bind(signal_handler, _1, boost::ref(cmd)));

		boost::thread_group tg;
		for (size_t i = 1; i < concurrency; ++i)
			tg.create_thread(boost::bind(&boost::asio::io_service::run, &ios));

		ios.run();
		tg.join_all();

	} catch(opmip::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;

	} catch(std::exception& e) {
		std::cerr << "exception: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

#include <boost/asio/impl/src.hpp>

// EOF //////////////////////////////////////////////////////////////////////////////////////////////////////
