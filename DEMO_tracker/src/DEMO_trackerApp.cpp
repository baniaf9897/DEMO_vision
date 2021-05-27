#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"

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

	void drawSpectrumPlot(const vector<float>& magSpectrum);


//	std::shared_ptr<Sender> sender;
	std::shared_ptr<OSCServer>			m_server;
	audio::InputDeviceNodeRef			m_inputNode;
	audio::MonitorSpectralNodeRef		m_monitorSpectralNode;
	vector<float>						mMagSpectrum;


};

void DEMO_trackerApp::setup()
{
	m_server = std::make_shared<OSCServer>(3339);
	
	
	
	auto ctx = ci::audio::master();

	m_inputNode = ctx->createInputDeviceNode();

	m_monitorSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode());

	m_inputNode >> m_monitorSpectralNode;
	m_inputNode->enable();
	ctx->enable();
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
	mMagSpectrum = m_monitorSpectralNode->getMagSpectrum();
	for (auto& i : mMagSpectrum) {
		console() << i << std::endl;
	}
}

void DEMO_trackerApp::draw()
{
	gl::clear();
	gl::enableAlphaBlending();

	drawSpectrumPlot(mMagSpectrum);

}

void DEMO_trackerApp::drawSpectrumPlot(const vector<float>& magSpectrum) 	
{
		if (magSpectrum.empty())
			return;

		gl::ScopedGlslProg glslScope(getStockShader(gl::ShaderDef().color()));

		ColorA bottomColor(0, 0, 1, 1);

		float width = getWindowWidth();
		float height = getWindowHeight();
		size_t numBins = magSpectrum.size();
		float padding = 0;
		float binWidth = (width - padding * (numBins - 1)) / (float)numBins;

		gl::VertBatch batch(GL_TRIANGLE_STRIP);

		size_t currVertex = 0;
		float m;
		Rectf bin(getWindowBounds().x1, getWindowBounds().y1, getWindowBounds().x1 + binWidth, getWindowBounds().y2);
		for (size_t i = 0; i < numBins; i++) {
			m = magSpectrum[i];
			//if (mScaleDecibels)
			m = audio::linearToDecibel(m) / 100;

			bin.y1 = bin.y2 - m * height;

			batch.color(bottomColor);
			batch.vertex(bin.getLowerLeft());
			batch.color(0, m, 0.7f);
			batch.vertex(bin.getUpperLeft());

			bin += vec2(binWidth + padding, 0);
			currVertex += 2;
		}

		batch.color(bottomColor);
		batch.vertex(bin.getLowerLeft());
		batch.color(0, m, 0.7f);
		batch.vertex(bin.getUpperLeft());

		gl::color(0, 0.9f, 0);

		batch.draw();

	}


CINDER_APP( DEMO_trackerApp, RendererGl )
