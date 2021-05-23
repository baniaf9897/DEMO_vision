#include "Sender.h"

Sender::Sender() {
	m_server = std::make_shared<OSCServer>(3333);
};

Sender::~Sender() {};

void Sender::sendValue(float value) {
	ci::osc::Message msg("/value/");
	msg.append((float)value);
	

	m_server->sendMsg(msg);


}