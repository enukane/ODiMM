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

#ifndef OPMIP_PMIP_LMA__HPP_
#define OPMIP_PMIP_LMA__HPP_

///////////////////////////////////////////////////////////////////////////////
#include <opmip/base.hpp>
#include <opmip/pmip/bcache.hpp>
#include <boost/bind.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/scoped_ptr.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace opmip { namespace pmip {

///////////////////////////////////////////////////////////////////////////////
class lma {
	typedef boost::asio::io_service::strand                  strand;
	typedef boost::scoped_ptr<boost::asio::io_service::work> work_ptr;

public:
	lma(boost::asio::io_service& ios)
		: _service(ios)
	{ }

	void start()
	{
		_service.dispatch(boost::bind(&lma::istart, this));
	}

	void stop()
	{
		_service.dispatch(boost::bind(&lma::istop, this));
	}

private:
	void istart()
	{
		_work.reset(new boost::asio::io_service::work(_service.get_io_service()));
	}

	void istop()
	{
		_work.reset(nullptr);
	}

private:
	strand       _service;
	work_ptr     _work;
	bcache::type _bcache;
};

///////////////////////////////////////////////////////////////////////////////
} /* namespace pmip */ } /* namespace opmip */

// EOF ////////////////////////////////////////////////////////////////////////
#endif /* OPMIP_PMIP_LMA__HPP_ */
