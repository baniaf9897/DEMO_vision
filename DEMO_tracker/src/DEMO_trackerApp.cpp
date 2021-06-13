#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"
#include "cinder/Timer.h"

#include "OSCServer.h"

using namespace ci;
using namespace ci::app;
using namespace std;

enum InteractionState {
	IDLE,
	PASSIVE,
	ACTIVE
};


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
	audio::BufferRecorderNodeRef		m_recorderNode;

	//audio::FilterHighPassNodeRef		m_highPass;
	vector<float>						m_magSpectrum;
	vector<float>						m_prevMagSpectrum;

	float								m_spectralCentroid;// spectral mass point
	float								m_spectralFlux;//spectral change over time
	float								m_spectralSharpness; //ratio of high frequency energy compared to total energy
	float								m_volume;

	Timer								m_timer;
	InteractionState					m_state;

	vector<float>						highPassFilter(float cutoff, vector<float> spectrum);
	vector<float>						lowPassFilter(float cutoff, vector<float> spectrum);
	
	float								getSpectralFlux(vector<float> spectrum, vector<float> prevSpectrum);
	float								getSpectralSharpness(vector<float> spectrum);

	void								manageTimerBasedOnVolume(float volume);

	void								sendValues();

	std::string							getCurrentTime();

	const float							m_highPassCutoff = 20.0f;;
	const float							m_lowPassCutoff = 20000.0f;
	const float							m_volumeThresholdPassive = 40.0f;
	const float							m_volumeThresholdActive  = 55.0f;
	const float							m_timeThresholdActive = 0.5f;;
	


};

void DEMO_trackerApp::setup()
{

	m_state = IDLE;

	m_server = std::make_shared<OSCServer>(3339);
	
	auto ctx = ci::audio::master();

	m_inputNode = ctx->createInputDeviceNode();
	m_monitorSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode());
	m_recorderNode = ctx->makeNode(new audio::BufferRecorderNode());
	m_recorderNode->setNumSeconds(10);
	
	//m_highPass = ctx->makeNode(new audio::FilterHighPassNode);
	//m_highPass->setCutoffFreq(m_highPassCutoff);

	m_inputNode  >> m_monitorSpectralNode >> m_recorderNode;
	m_inputNode->enable();
	ctx->enable();

}

void DEMO_trackerApp::mouseDown( MouseEvent event )
{
	
}

void DEMO_trackerApp::manageTimerBasedOnVolume(float volume) {
	if (volume > m_volumeThresholdActive) {

		if (m_timer.isStopped()) {
			m_timer.start();
			//record audio

		};
	}
	else {
		if (!m_timer.isStopped()) {
			m_timer.stop();
			//end recording
		}
	}

}

void DEMO_trackerApp::update()
{
	
	m_volume = audio::linearToDecibel(m_monitorSpectralNode->getVolume());

	if (m_volume > m_volumeThresholdPassive) {

		m_state = PASSIVE;

		m_prevMagSpectrum = m_magSpectrum;

		//filtering
		m_magSpectrum = highPassFilter(m_highPassCutoff, m_monitorSpectralNode->getMagSpectrum());
		m_magSpectrum = lowPassFilter(m_lowPassCutoff, m_magSpectrum);
		
		//calculating values
		m_spectralCentroid = m_monitorSpectralNode->getSpectralCentroid();
		m_spectralFlux = getSpectralFlux(m_magSpectrum,m_prevMagSpectrum);
		m_spectralSharpness = getSpectralSharpness(m_magSpectrum);

		
		if (m_volume > m_volumeThresholdActive) {

			if (m_timer.isStopped()) {
				m_timer.start();
			}
			else {
				if (m_timer.getSeconds() > m_timeThresholdActive) {


					if (m_recorderNode->getWritePosition() == 0) {
						m_recorderNode->start();
					}
					m_state = ACTIVE;
				}
			}
		}
		else {
			if (!m_timer.isStopped()) {
				m_timer.stop();
				m_recorderNode->stop();
				std::string fileName = "audio/" + getCurrentTime() + ".wav";
				m_recorderNode->writeToFile(fileName);
			}
		}

		
		sendValues();
	}
	else {
		m_state = IDLE;
	};

}

void DEMO_trackerApp::draw()
{
	gl::clear();
	gl::enableAlphaBlending();

	drawSpectrumPlot(m_magSpectrum);

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

vector<float>	DEMO_trackerApp::highPassFilter(float cutoff, vector<float> spectrum) {

	auto ctx = ci::audio::master();

	int binIndex = cutoff * (float)m_monitorSpectralNode->getFftSize()  / (float)ctx->getSampleRate() ;

	for (int i = 0; i <= binIndex; i++) {
		spectrum[i] = 0.0f;
	}

	return spectrum;
}

vector<float>	DEMO_trackerApp::lowPassFilter(float cutoff, vector<float> spectrum) {
	auto ctx = ci::audio::master();

	int binIndex = cutoff * (float)m_monitorSpectralNode->getFftSize() / (float)ctx->getSampleRate();

	for (int i = binIndex; i < spectrum.size(); i++) {
		spectrum[i] = 0.0f;
	}

	return spectrum;
}

float	DEMO_trackerApp::getSpectralFlux(vector<float> spectrum, vector<float> prevSpectrum) {

	if (spectrum.size() != prevSpectrum.size()) {
		return 0.0f;
	}

	float flux = 0.0f;

	for (int i = 0; i < spectrum.size(); i++) {
		flux += audio::linearToDecibel(abs(spectrum[i] - prevSpectrum[i]));
	};

	return flux;
}
float	DEMO_trackerApp::getSpectralSharpness(vector<float> spectrum) {
	float sharpness = 0.0f;

	for (int i = 0; i < spectrum.size(); i++) {
		sharpness += i * audio::linearToDecibel(spectrum[i]);
	};

	sharpness /= spectrum.size();

	return sharpness;
}
void 	DEMO_trackerApp::sendValues() {
	
	//active flag
	//m_spectralCentroid
	//m_spectralFlux 
	//m_spectralSharpness
	
	ci::osc::Message msg("/value/");
	
	if(m_state == ACTIVE)
		msg.append((int)1);
	else
		msg.append((int)0);

	msg.append((float)m_spectralCentroid);
	msg.append((float)m_spectralFlux);
	msg.append((float)m_spectralSharpness);

	console() << "Active   " << m_state << std::endl;
	console() << "Centroid " << m_spectralCentroid << std::endl;
	console() << "Flux     " << m_spectralFlux << std::endl;
	console() << "Sharpness" << m_spectralSharpness << std::endl;

	m_server->sendMsg(msg);
	
}

std::string	DEMO_trackerApp::getCurrentTime() {
	time_t rawtime;
	struct tm* timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

		strftime(buffer, sizeof(buffer), "%d-%m-%Y/%H.%M.%S", timeinfo);
	 std::string str(buffer);
	 return str;
}


CINDER_APP( DEMO_trackerApp, RendererGl )
