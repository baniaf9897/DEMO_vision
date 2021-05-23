#include "OSCServer.h"
using namespace asio;
using namespace asio::ip;

OSCServer::OSCServer(uint16_t localPort)
	: mSocket(new udp::socket(ci::app::App::get()->io_context() , udp::endpoint(udp::v4(), localPort - 1))),
	// The endpoint that we want to "send" to is the v4 broadcast address.
	mSender(mSocket, udp::endpoint(address_v4::broadcast(), localPort)), mIsConnected(false)
{
	// Set the socket option for broadcast to true.
	mSocket->set_option(asio::socket_base::broadcast(true));
	mIsConnected = true;
}


OSCServer::~OSCServer()
{
	mSender.close();
	mSocket->close();
}

void OSCServer::sendMsg(ci::osc::Message msg)
{
	mSender.send(msg, std::bind(&OSCServer::onSendError,
		this, std::placeholders::_1));
}

void OSCServer::onSendError(asio::error_code error)
{
	if (error) {
		mIsConnected = false;
		try {
			// Close the socket on exit. This function could throw. The exception will
			// contain asio::error_code information.
			mSender.close();
		}
		catch (const ci::osc::Exception& ex) {
		}
	}
}
