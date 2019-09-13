//
// ping.cpp
// ~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <istream>
#include <iostream>
#include <ostream>

#include "C:\ProjectNm\boost_1_71_0\libs\asio\example\cpp03\icmp\icmp_header.hpp"
#include "C:\ProjectNm\boost_1_71_0\libs\asio\example\cpp03\icmp\ipv4_header.hpp"

using boost::asio::ip::icmp;
using boost::asio::deadline_timer;
using boost::asio::io_service;
using boost::asio::streambuf;
using boost::system::error_code;
using std::cout;
using std::endl;
namespace posix_time = boost::posix_time;

static const std::string BODY = "ping";
static const auto PROCESS = GetCurrentProcessId();

static int gSequence;
static io_service gService;
static icmp::socket gSocket(gService, icmp::v4());
static deadline_timer gTimer(gService);
static streambuf gReply;
static icmp::endpoint gReceiver;

void StartReceive()
{
	gSocket.async_receive_from(gReply.prepare(65536), gReceiver,
		[&](const error_code& error, size_t length)
		{
			gReply.commit(length);

			ipv4_header ipv4Hdr;
			icmp_header icmpHdr;
			std::string body(BODY.size(), 0);

			std::istream is(&gReply);
			is >> ipv4Hdr >> icmpHdr;
			is.read(&body[0], BODY.size());

			auto ip = ipv4Hdr.source_address().to_string();
			auto rc = gReceiver.address().to_string();
			auto id = icmpHdr.identifier();
			auto process = PROCESS;
			auto sn = icmpHdr.sequence_number();
			auto type = icmpHdr.type();

			cout << "  Length              = " << length << endl;
			cout << "  Error               = " << error << endl;
			cout << "  IP checksum         = " << ipv4Hdr.header_checksum() << endl;
			cout << "  IP address          = " << ip << endl;
			cout << "  Receiver address    = " << rc << endl;
			cout << "  ICMP identification = " << id << endl;
			cout << "  ICMP type           = " << (int)type << endl;
			cout << "  Process             = " << process << endl;
			cout << "  Sequence            = " << sn << endl;

			if (is
				&& icmpHdr.type() == icmp_header::echo_reply
				&& icmpHdr.identifier() == PROCESS
				&& icmpHdr.sequence_number() == gSequence
				&& body == BODY)
			{
				cout << "    > " << ip<<"\n Status OK" << endl;
			}

			cout << endl;

			gReply.consume(length);

			StartReceive();
		});
}

int main()
{
	icmp::resolver resolver(gService);

	icmp_header echoRequest;
	echoRequest.type(icmp_header::echo_request);
	echoRequest.identifier(PROCESS);

	for (gSequence = 0; gSequence < 1; gSequence++)
	{
		cout << "----------------------------------------------------------" << endl;
		cout << "Iteration = " << gSequence << endl;
		cout << "----------------------------------------------------------" << endl;

		echoRequest.sequence_number(gSequence);
		compute_checksum(echoRequest, BODY.begin(), BODY.end());

		streambuf request;
		std::ostream os(&request);
		os << echoRequest << BODY;

		gService.reset();

		StartReceive();

		std::vector<std::string> pool
		{
			"192.168.1.69",
			"192.168.1.118",
			"192.168.1.1"
		};

		for (const auto & ip : pool)
		{
			icmp::resolver::query query(icmp::v4(), ip, "");
			auto dest = *resolver.resolve(query);

			gSocket.send_to(request.data(), dest);
		}

		gTimer.expires_from_now(posix_time::millisec(2000));
		gTimer.async_wait([&](const error_code& error) { gService.stop(); });

		gService.run();
		gReply.commit(gReply.size());
		gReply.consume(gReply.size());
	}

	return 0;
}