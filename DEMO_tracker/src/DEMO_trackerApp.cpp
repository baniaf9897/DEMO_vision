#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"
#include "cinder/Timer.h"

#include "OSCServer.h"
#include "cinder/params/Params.h"

using namespace ci;
using namespace ci::app;
using namespace std;

enum InteractionState {
	IDLE,
	PASSIVE,
	ACTIVE
};

void prepareSettings(App::Settings* settings)
{
	settings->setHighDensityDisplayEnabled();
}


class DEMO_trackerApp : public App {
  public:
	void setup() override;
	void update() override;
	void draw() override;

	void drawSpectrumPlot(const vector<float>& magSpectrum);


//	std::shared_ptr<Sender> sender;
	std::shared_ptr<OSCServer>			m_server;
	audio::InputDeviceNodeRef			m_inputNode;
	audio::MonitorSpectralNodeRef		m_monitorSpectralNode;
	audio::BufferRecorderNodeRef		m_recorderNode;

	audio::FilterBandPassNodeRef		m_bandPassNode;
	audio::FilterHighPassNodeRef		m_highPassNode;
	audio::FilterLowPassNodeRef			m_lowPassNode;
	audio::DelayNodeRef					m_delayNode;
	
	params::InterfaceGlRef				m_params;

	//audio::FilterHighPassNodeRef		m_highPass;
	vector<float>						m_magSpectrum;
	vector<float>						m_prevMagSpectrum;

	float								m_spectralCentroid;// spectral mass point
	float								m_spectralFlux;//spectral change over time
	float								m_spectralSharpness; //ratio of high frequency energy compared to total energy
	float								m_volume;


	static const int					m_filterLength = 15;
	static const int					m_filterLengthVolume = 50;

	float								m_spectralCentroidBuffer[m_filterLength];
	float								m_spectralFluxBuffer[m_filterLength];
	float								m_spectralSharpnessBuffer[m_filterLength];
	float								m_volumeBuffer[m_filterLengthVolume];

	int									m_currentIndex;
	int									m_currentVolumeIndex;

	Timer								m_timer;
	Timer								m_timerActiveInteraction;
	Timer								m_coolDownTimer;

	InteractionState					m_state;

	vector<float>						highPassFilter(float cutoff, vector<float> spectrum);
	vector<float>						lowPassFilter(float cutoff, vector<float> spectrum);

	vector<float>						volumeFilter(vector<float> spectrum);
	
	float								getSpectralFlux(vector<float> spectrum, vector<float> prevSpectrum);
	float								getSpectralSharpness(vector<float> spectrum);
	float								getSpectralBrigthness();

	void								sendValues();

	std::string							getCurrentTime();

	float								normalize(float min, float max, float value);

	void								calcValues();

	float								m_highPassCutoff = 500.0f;;
	float								m_lowPassCutoff = 20000.0f;

	float								m_volumeThresholdPassive = 40.0f;
	float								m_volumeThresholdActive  = 65.0f;
	float								m_timeThresholdActive = 1.f;

	float								m_volumeCutoffLow = 20.0f;
	float								m_volumeCutoffHigh = 80.0f;

	float								m_minFlux = 0.0f;
	float								m_maxFlux = 50.0f;

	float								m_minSharpness = 10.0f;
	float								m_maxSharpness = 100.0f;

	float								m_minBrightness = 10.0f;
	float								m_maxBrightness = 7000.0f;


	int									m_minActiveInteractionTime = 20;
	int									m_coolDownTime = 3;

	float								m_delay = 1.0f;

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

	m_highPassNode = ctx->makeNode(new audio::FilterHighPassNode());
	m_highPassNode->setFreq(m_highPassCutoff);

	m_lowPassNode = ctx->makeNode(new audio::FilterLowPassNode());
	m_lowPassNode->setFreq(m_lowPassCutoff/4.0f);

	m_delayNode = ctx->makeNode(new audio::DelayNode());
	m_delayNode->setDelaySeconds(m_delay);
	m_delayNode->setMaxDelaySeconds(m_delay);

	m_inputNode  >> m_monitorSpectralNode >> m_recorderNode >> m_highPassNode >> m_lowPassNode >> m_delayNode >> ctx->getOutput();
	m_inputNode->enable();
	ctx->enable();

	m_currentIndex = -1;

	m_params = params::InterfaceGl::create(getWindow(), "App parameters", toPixels(ivec2(300, 400)));


	// Setup some basic parameters.
	m_params->addParam("HighPass Cutoff", &m_highPassCutoff);
	m_params->addParam("LowPass Cutoff", &m_lowPassCutoff);

	m_params->addParam("Volume Cutoff Low", &m_volumeCutoffLow);
	m_params->addParam("Volume Cutoff High", &m_volumeCutoffHigh);

	m_params->addSeparator();

	m_params->addParam("Passive Interaction Threshold (Volume)", &m_volumeThresholdPassive);
	m_params->addParam("Active Interaction Threshold (Volume)", &m_volumeThresholdActive);
	m_params->addParam("Active Interaction Threshold (Time)", &m_timeThresholdActive);
	m_params->addSeparator();
	
	m_params->addParam("Volume Cutoff Low", &m_volumeCutoffLow);
	m_params->addParam("Volume Cutoff High", &m_volumeCutoffHigh);
	m_params->addSeparator();

	m_params->addParam("Min Flux", &m_minFlux);
	m_params->addParam("Max Flux", &m_maxFlux);
	m_params->addSeparator();

	m_params->addParam("Min Brightness", &m_minBrightness);
	m_params->addParam("Max Brightness", &m_maxBrightness);
	m_params->addSeparator();

	m_params->addParam("Min Sharpness", &m_minSharpness);
	m_params->addParam("Max Sharpness", &m_maxSharpness);
	m_params->addSeparator();

	m_params->addParam("Min Interaction Time", &m_minActiveInteractionTime);
	m_params->addParam("CoolDown Time", &m_coolDownTime);
	m_params->addSeparator();

	m_params->addParam("Delay",&m_delay).updateFn([this] { m_delayNode->setDelaySeconds(m_delay);  m_delayNode->setMaxDelaySeconds(m_delay); });

}

void DEMO_trackerApp::calcValues() {
	m_prevMagSpectrum = m_magSpectrum;

	//filtering
	m_magSpectrum = highPassFilter(m_highPassCutoff, m_monitorSpectralNode->getMagSpectrum());
	m_magSpectrum = lowPassFilter(m_lowPassCutoff, m_magSpectrum);
	m_magSpectrum = volumeFilter(m_magSpectrum);
	//calculating values
	m_spectralCentroid = getSpectralBrigthness();
	m_spectralFlux = getSpectralFlux(m_magSpectrum, m_prevMagSpectrum);
	m_spectralSharpness = getSpectralSharpness(m_magSpectrum);

	m_currentIndex = (m_currentIndex + 1) % m_filterLength;
	m_currentVolumeIndex = (m_currentVolumeIndex + 1) % m_filterLengthVolume;

	m_spectralCentroidBuffer[m_currentIndex] = m_spectralCentroid;
	m_spectralFluxBuffer[m_currentIndex] = m_spectralFlux;
	m_spectralSharpnessBuffer[m_currentIndex] = m_spectralSharpness;

	
	m_volumeBuffer[m_currentVolumeIndex] = m_volume;
}




void DEMO_trackerApp::update()
{
	InteractionState newState = IDLE;
	m_volume = audio::linearToDecibel(m_monitorSpectralNode->getVolume());



	if (!m_coolDownTimer.isStopped() && m_coolDownTimer.getSeconds() < m_coolDownTime) {
		newState = PASSIVE;
		sendValues();

		return;
	}
	else {
		m_coolDownTimer.stop();
	};




	if (m_volume > m_volumeThresholdPassive) {

		newState = PASSIVE;

		calcValues();

		float avgVolume = 0.0f;

		for (int i = 0; i < m_filterLengthVolume; i++) {
			avgVolume += m_volumeBuffer[i];
		};

		avgVolume /= m_filterLengthVolume;

		if (avgVolume > m_volumeThresholdActive) {
			newState = ACTIVE;
			/*if (m_timer.isStopped()) {
				m_timer.start();
			}
			else {
				if (m_timer.getSeconds() > m_timeThresholdActive) {
				
					if (m_recorderNode->getWritePosition() == 0) {
						m_recorderNode->start();
					}
					newState = ACTIVE;
				}
			}*/
		}
		else {
			if (m_state == ACTIVE) {
				if (m_timerActiveInteraction.isStopped()) {
					m_timerActiveInteraction.start();
				}
				
				if (m_timerActiveInteraction.getSeconds() < m_minActiveInteractionTime) {
					newState = ACTIVE;
				}else{
					console() << "Stop Active Interaction" << std::endl;
					m_timerActiveInteraction.stop();
					//m_timer.stop();
					m_recorderNode->stop();
					//std::string fileName = "audio/" + getCurrentTime() + ".wav";
					
					if (m_coolDownTimer.isStopped()) {
						m_coolDownTimer.start();
					};
				
				}
				
				//m_recorderNode->writeToFile(fileName);
			}
		}

		
		sendValues();
	}
	

	m_state = newState;

}

void DEMO_trackerApp::draw()
{
	gl::clear();
	gl::enableAlphaBlending();

	drawSpectrumPlot(m_magSpectrum);
	m_params->draw();
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


vector<float>	DEMO_trackerApp::volumeFilter(vector<float> spectrum) {
	for (int i = 0; i < spectrum.size(); i++) {
		
		if(audio::linearToDecibel(spectrum[i]) < m_volumeCutoffLow || audio::linearToDecibel(spectrum[i])> m_volumeCutoffHigh)
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

	flux /= (float)spectrum.size();

	return normalize(m_minFlux,m_maxFlux,flux);
}
float	DEMO_trackerApp::getSpectralSharpness(vector<float> spectrum) {
	float weightedSum = 0.0f;
	float sum = 0.0f;

	for (int i = 0; i < spectrum.size(); i++) {
		weightedSum += i * audio::linearToDecibel(spectrum[i]);
		sum += audio::linearToDecibel(spectrum[i]);
	};

	if(sum > 0)
		return  normalize(m_minSharpness,m_maxSharpness, weightedSum / sum);

	return 0.0f;
}

float	DEMO_trackerApp::getSpectralBrigthness() {
	float centroid = m_monitorSpectralNode->getSpectralCentroid();
	return normalize(m_minBrightness, m_maxBrightness, centroid);
}

float	DEMO_trackerApp::normalize(float min, float max, float value) {
	float v =  (value - min) / (max - min);

	if (v < 0.0f)
		return 0.0f;
	else if (v > 1.0f)
		return 1.0f;
	else
		return v;
}



void 	DEMO_trackerApp::sendValues() {
	
	//active flag
	//m_spectralCentroid
	//m_spectralFlux 
	//m_spectralSharpness  

	float avgSpectralCentroid = 0.0f;
	float avgSpectralFlux = 0.0f;
	float avgSpectralSharpness = 0.0f;

	for (int i = 0; i < m_filterLength; i++) {
		avgSpectralCentroid += m_spectralCentroidBuffer[i];
		avgSpectralFlux += m_spectralFluxBuffer[i];
		avgSpectralSharpness += m_spectralSharpnessBuffer[i];
	}

	avgSpectralCentroid /= m_filterLength;
	avgSpectralFlux /= m_filterLength;
	avgSpectralSharpness /= m_filterLength;

	ci::osc::Message msg("/value/");
	
	if(m_state == ACTIVE)
		msg.append((int)1);
	else
		msg.append((int)0);

	msg.append((float)avgSpectralCentroid);
	msg.append((float)avgSpectralFlux);
	msg.append((float)avgSpectralSharpness);
	msg.append((float)normalize(m_volumeCutoffLow, m_volumeCutoffHigh, m_volume));

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

CINDER_APP( DEMO_trackerApp, RendererGl, prepareSettings)
