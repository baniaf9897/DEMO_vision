#pragma once
#include "OSCServer.h"

class Sender
{
public:
	Sender(int port);
	~Sender();

	void sendValue(float value);

private:
	std::shared_ptr<OSCServer>			m_server;

};

