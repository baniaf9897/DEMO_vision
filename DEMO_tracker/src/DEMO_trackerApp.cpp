#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "OSCServer.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class DEMO_trackerApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

//	std::shared_ptr<Sender> sender;
	std::shared_ptr<OSCServer>			m_server;


};

void DEMO_trackerApp::setup()
{
	m_server = std::make_shared<OSCServer>(3339);
}

void DEMO_trackerApp::mouseDown( MouseEvent event )
{
	ci::osc::Message msg("/value/");
	msg.append((float)1.0f);


	m_server->sendMsg(msg);

	console() << "Send value" << std::endl;
}

void DEMO_trackerApp::update()
{
}

void DEMO_trackerApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( DEMO_trackerApp, RendererGl )
