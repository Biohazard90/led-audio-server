
#pragma once

#include <mmeapi.h>

// Output
#define MAX_BARS 64
#define BAR_VOLUME_SCALE 0.001f

struct IAudioClient;
struct IAudioCaptureClient;
struct IMMDeviceEnumerator;

typedef int AUDIO_EVENT_HANDLER_TOKEN;

typedef std::function<void(float *buffer64)> FnAudioCallback;

//typedef int (WINAPI *FnCallNtPowerInformation)(_In_ POWER_INFORMATION_LEVEL InformationLevel, _In_ PVOID lpInputBuffer,
//	_In_ ULONG nInputBufferSize, _Out_ PVOID lpOutputBuffer, _In_ ULONG nOutputBufferSize);

namespace
{
	struct WASAPIEventBase;
}

struct AudioProcessingParams
{
	AudioProcessingParams()
		: spreadPower(0.25f)
		, hammingParam(0.501f)
		, timeDomainSizeScale(30.0f)
		, frequencyDomainSizeScale(10.0f)
	{}

	float spreadPower;
	float hammingParam;

	float timeDomainSizeScale;
	float frequencyDomainSizeScale;
};

class AudioEventHandler
{
public:
	AudioEventHandler();
	~AudioEventHandler();

	void Shutdown();

	AUDIO_EVENT_HANDLER_TOKEN AddDefaultOutputChangedCallback(std::function<void()> callback);
	void RemoveDefaultOutputChangedCallback(AUDIO_EVENT_HANDLER_TOKEN token);

private:
	void OnDefaultDeviceChanged();

private:
	std::unordered_map<AUDIO_EVENT_HANDLER_TOKEN, std::function<void()>> callbacks;
	int nextCallback = 0;
	std::recursive_mutex callbacksMutex;

	IMMDeviceEnumerator *pEnumerator = nullptr;
	WASAPIEventBase *eventBase = nullptr;
};

class AudioProcessor
{
public:
	AudioProcessor();
	~AudioProcessor();

	static int GetDefaultDeviceIndex();

	void StartRecording(const std::string &deviceId);
	void StopRecording();
	bool IsRecording() const { return isRecording; }
	const char *GetCurrentDevice() { return currentDevice.c_str(); }

	void SetInputVolume(float volume) { this->volume = volume; }
	void SetInputThreshold(float threshold) { this->threshold = threshold; }
	void SetMaxFPS(int maxFPS);
	void SetCallback(FnAudioCallback callback) { this->callback = callback; }

	void ReadAudioBufferStereo64(float *bars, int &audioBufferFlags);
	void OnDefaultDeviceChanged();

	void SetAudioProcessingParams(const AudioProcessingParams &audioProcessingParams) { this->audioProcessingParams = audioProcessingParams; }

private:
	void SampleThread();

private:
	float *outputBuffer;
	int deviceFlags;
	float volume;
	float threshold;
	int sampleSleepTimer;
	//HANDLE audioAvailabilityEvent;
	std::mutex outputBufferMutex;
	std::thread audioSampleThread;
	std::mutex audioSampleThreadStopCondition;
	IMMDeviceEnumerator *pEnumerator;
	IAudioClient *pAudioClient;
	IAudioCaptureClient *pCaptureClient;
	int channelCount;
	AudioProcessingParams audioProcessingParams;
	uint32 numFramesFFT;
	uint32 numBinsFFT;

	bool isRecording;
	std::string currentDevice;
	FnAudioCallback callback;

	WASAPIEventBase *eventBase;
	bool defaultDeviceWasChanged;

	//HMODULE powerProfModule;
	//FnCallNtPowerInformation pFnCallNtPowerInformation;
};
