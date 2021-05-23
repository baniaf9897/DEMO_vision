#pragma once

#include "Osc.h"


using Sender = ci::osc::SenderUdp;
#define _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
class OSCServer
{
public:
	OSCServer(uint16_t localPort);
	~OSCServer();

	inline bool isRunning() { return mIsConnected; }

	void sendMsg(ci::osc::Message msg);

private:
	ci::osc::UdpSocketRef	mSocket;
	Sender	mSender;
	bool	mIsConnected;
	void	onSendError(asio::error_code error);
};

