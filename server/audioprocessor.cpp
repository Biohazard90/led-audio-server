
#include "platform.h"

#include "audioprocessor.h"

#include "ffts.h"
#include "timer.h"
//#include "io/mediaextensions.h"
//#include "json.h"
//#include "render/rendercontext.h"
//#include "util/stringutil.h"

#include <Dshow.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <limits>
#include <malloc.h>

using namespace std;

namespace
{
#define WASAPI_DEBUG 1
#if WASAPI_DEBUG
#define HANDLE_WASAPI_DEVICE_ENUM_ERROR(hr, msg) { if (FAILED(hr)){ WPX_Error("WASAPI error, line %d: %s. HRESULT: %x.\n", __LINE__, msg, hr); goto ExitWASAPIDeviceEnumeration;} }
#define HANDLE_WASAPI_INIT_ERROR(hr, msg) { if (FAILED(hr)){ WPX_Error("WASAPI error, line %d: %s. HRESULT: %x.\n", __LINE__, msg, hr); goto ExitWASAPIInit;} }
#define HANDLE_WASAPI_INIT_ERROR_NOEXIT(hr, msg) { if (FAILED(hr)){ WPX_Error("WASAPI error, line %d: %s. HRESULT: %x.\n", __LINE__, msg, hr);} }
#define HANDLE_WASAPI_DEVICE_SAMPLE_ERROR(hr, msg) { if (FAILED(hr)){ WPX_Error("WASAPI error, line %d: %s. HRESULT: %x.\n", __LINE__, msg, hr); shouldRestart = true; break; } }
#else
#define HANDLE_WASAPI_DEVICE_ENUM_ERROR(hr, msg) { if (FAILED(hr)) goto ExitWASAPIDeviceEnumeration; }
#define HANDLE_WASAPI_INIT_ERROR(hr, msg) { if (FAILED(hr)) goto ExitWASAPIInit; }
#define HANDLE_WASAPI_DEVICE_SAMPLE_ERROR(hr, msg) { if (FAILED(hr)){ shouldRestart = true; break; } }
#endif

	const CLSID _CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID _IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID _IID_IAudioClient = __uuidof(IAudioClient);
	const IID _IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

	IMMDevice *GetAudioEndpointById(const string &deviceId, IMMDeviceEnumerator *pEnumerator, bool &isLoopbackDevice)
	{
		HRESULT hr;
		IMMDeviceCollection *pCollection = nullptr;
		IMMDevice *pEndpoint = nullptr;
		LPWSTR pwszID = nullptr;

		EDataFlow modes[] = { eRender, eCapture };
		for (int m = 0; m < ARRAYSIZE(modes); ++m)
		{
			hr = pEnumerator->EnumAudioEndpoints(
				modes[m], DEVICE_STATE_ACTIVE,
				&pCollection);
			HANDLE_WASAPI_DEVICE_ENUM_ERROR(hr, "EnumAudioEndpoints failed");

			UINT  count;
			hr = pCollection->GetCount(&count);
			HANDLE_WASAPI_DEVICE_ENUM_ERROR(hr, "pCollection->GetCount failed");

			for (ULONG i = 0; i < count; i++)
			{
				// Get pointer to endpoint number i.
				hr = pCollection->Item(i, &pEndpoint);
				HANDLE_WASAPI_DEVICE_ENUM_ERROR(hr, "pCollection->Item failed");

				hr = pEndpoint->GetId(&pwszID);
				HANDLE_WASAPI_DEVICE_ENUM_ERROR(hr, "pEndpoint->GetId failed");

				if (TO_UTF8(pwszID) == deviceId)
				{
					// Found correct device
					isLoopbackDevice = modes[m] == eRender;
					goto ExitWASAPIDeviceEnumerationSuccess;
				}

				// Finish this iteration
				CoTaskMemFree(pwszID);
				pwszID = nullptr;
				SafeRelease(&pEndpoint);
			}
			SafeRelease(&pCollection);
		}

	ExitWASAPIDeviceEnumeration:
		SafeRelease(&pEndpoint);

	ExitWASAPIDeviceEnumerationSuccess:
		CoTaskMemFree(pwszID);
		SafeRelease(&pCollection);
		return pEndpoint;
	}

	bool StartWASAPI(const AudioProcessingParams &audioProcessingParams, const string &deviceId, int &channelCount, int &deviceFlags, uint32 &numFramesFFT, uint32 &numBinsFFT,
		IMMDeviceEnumerator **pEnumerator, IAudioClient **pAudioClient, IAudioCaptureClient **pCaptureClient)
	{
		deviceFlags = 0;
		bool success = false;
		HRESULT hr;
		//REFERENCE_TIME hnsActualDuration;
		//UINT32 bufferFrameCount;
		IMMDevice *pDevice = nullptr;
		WAVEFORMATEX *pwfx = nullptr;
		bool isLoopbackDevice = true;

		// The audio interfaces are called from various threads though..
		//hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		//HANDLE_WASAPI_INIT_ERROR(hr, "CoInitializeEx failed");

		if (*pEnumerator == nullptr)
		{
			hr = CoCreateInstance(
				_CLSID_MMDeviceEnumerator, nullptr,
				CLSCTX_ALL, _IID_IMMDeviceEnumerator,
				(void**)pEnumerator);
			HANDLE_WASAPI_INIT_ERROR(hr, "CoCreateInstance failed");
		}

		//if (!deviceId.empty() && deviceId != JSONVAL_default)
		//{
		//	// Select a specific render or record device by ID
		//	pDevice = GetAudioEndpointById(deviceId, *pEnumerator, isLoopbackDevice);
		//}

		if (pDevice == nullptr)
		{
			// Select default render device
			hr = (*pEnumerator)->GetDefaultAudioEndpoint(
				eRender, eConsole, &pDevice);
			HANDLE_WASAPI_INIT_ERROR(hr, "pEnumerator->GetDefaultAudioEndpoint failed");
		}

		if (pDevice == nullptr)
		{
			HANDLE_WASAPI_INIT_ERROR(hr, "pDevice is null");
		}

#if 0
		IPropertyStore *pProps = nullptr;

		if (SUCCEEDED(pDevice->OpenPropertyStore(
			STGM_READ, &pProps)))
		{
			PROPVARIANT varName;
			// Initialize container for property value.
			PropVariantInit(&varName);

			// Get the endpoint's friendly-name property.
			if (SUCCEEDED(pProps->GetValue(
				PKEY_Device_FriendlyName, &varName)))
			{
				WPX_Msg("WASAPI open device: %S\n", varName.pwszVal);
			}
			SAFE_RELEASE(pProps);
			PropVariantClear(&varName);
		}
#endif

		hr = pDevice->Activate(
			_IID_IAudioClient, CLSCTX_ALL,
			nullptr, (void**)pAudioClient);
		HANDLE_WASAPI_INIT_ERROR(hr, "pDevice->Activate failed");

		if ((*pAudioClient) == nullptr)
			return false;

		hr = (*pAudioClient)->GetMixFormat(&pwfx);
		HANDLE_WASAPI_INIT_ERROR(hr, "(*pAudioClient)->GetMixFormat failed");

		if (pwfx->wBitsPerSample != 32)
		{
			WPX_FatalError("WASAPI processor requires 32 bit per sample.");
		}

		channelCount = pwfx->nChannels;

		//if (pwfx->wFormatTag == 0xFFFE)
		//{
		//	WAVEFORMATEXTENSIBLE *waveFormatExt = (WAVEFORMATEXTENSIBLE*)pwfx;
		//	waveFormatExt->dwChannelMask = 0x3;
		//}

		float temporalScale = MAX(1.0f, pwfx->nSamplesPerSec / 44100.0f);
		numFramesFFT = temporalScale * MAX_BARS * audioProcessingParams.timeDomainSizeScale;
		numBinsFFT = MAX_BARS * audioProcessingParams.frequencyDomainSizeScale;
		//numBinsFFT = MAX_BARS * (5.0f - 0.5f * temporalScale);

		if (pwfx->nBlockAlign != pwfx->nChannels * 4)
		{
			WPX_FatalError("WASAPI processor encountered unexpected block align: %i * %i != %i.", 4, channelCount, pwfx->nBlockAlign);
		}

		const REFERENCE_TIME requestedBufferSizes[] =
		{
			4 * 1024 * 1024,
			1024 * 1024,
			1024 * 256,
			0
		};

		//WPX_Msg("Input format: %i, %i, %i, %i, %i\n", pwfx->nBlockAlign, pwfx->nAvgBytesPerSec, pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample, pwfx->wFormatTag);
		//pwfx->nBlockAlign = 24;
		//pwfx->nAvgBytesPerSec = 1152000;
		//pwfx->nChannels = 6;
		//pwfx->nSamplesPerSec = 48000;
		//pwfx->wBitsPerSample = 32;
		//pwfx->wFormatTag = 65534;
		//WPX_Msg("Override format: %i, %i, %i, %i, %i\n", pwfx->nBlockAlign, pwfx->nAvgBytesPerSec, pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample, pwfx->wFormatTag);

		//WPX_Msg("%x\n", AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED);
		// 88890008
#if 1
		bool hasFoundFormat = false;
		for (int i = 0; i < ARRAYSIZE(requestedBufferSizes); ++i)
		{
			hr = (*pAudioClient)->Initialize(
				AUDCLNT_SHAREMODE_SHARED,
				isLoopbackDevice ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
				requestedBufferSizes[i],
				0,
				pwfx,
				nullptr);
			HANDLE_WASAPI_INIT_ERROR_NOEXIT(hr, "(*pAudioClient)->Initialize failed");

			// Valid format, quit searching.
			if (SUCCEEDED(hr))
			{
				hasFoundFormat = true;
				break;
			}
		}

		// Print format details if we failed.
		if (FAILED(hr))
		{
			WPX_Error("Failed default mix format: channels %i, block align %i, sample rate %i\n",
				pwfx->nChannels, pwfx->nBlockAlign, pwfx->nSamplesPerSec);
		}

		// Try stereo channel recording even when the format isn't matching in the way Microsoft wants
		if (!hasFoundFormat && isLoopbackDevice)
		{
			const DWORD channelSetups[] =
			{
				KSAUDIO_SPEAKER_STEREO,
				KSAUDIO_SPEAKER_MONO,
				KSAUDIO_SPEAKER_3POINT0,
				KSAUDIO_SPEAKER_QUAD,
				KSAUDIO_SPEAKER_SURROUND,
				KSAUDIO_SPEAKER_5POINT0,
				KSAUDIO_SPEAKER_5POINT1,
				KSAUDIO_SPEAKER_5POINT1_SURROUND,
				KSAUDIO_SPEAKER_7POINT1,
				KSAUDIO_SPEAKER_7POINT1_SURROUND,
			};
			const int channelSetupCount = ARRAYSIZE(channelSetups);
			const int channelTests[channelSetupCount] = { 2, 1, 3, 4, 4, 5, 6, 6, 8, 8 };

			WAVEFORMATEXTENSIBLE *waveFormatExt = (pwfx->wFormatTag == 0xFFFE) ? (WAVEFORMATEXTENSIBLE*)pwfx : nullptr;
			const DWORD originalChannelMask = (waveFormatExt != nullptr) ? waveFormatExt->dwChannelMask : 0;

			for (int c = 0; c < ARRAYSIZE(channelTests) && !hasFoundFormat; ++c)
			{
				channelCount = channelTests[c];

				pwfx->nChannels = channelCount;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nSamplesPerSec * pwfx->nBlockAlign;

				if (waveFormatExt != nullptr)
				{
					waveFormatExt->dwChannelMask = channelSetups[c];
				}

				for (int i = 0; i < ARRAYSIZE(requestedBufferSizes); ++i)
				{
					hr = (*pAudioClient)->Initialize(
						AUDCLNT_SHAREMODE_SHARED,
						AUDCLNT_STREAMFLAGS_LOOPBACK,
						requestedBufferSizes[i],
						0,
						pwfx,
						nullptr);

					if (FAILED(hr) && originalChannelMask != 0)
					{
						((WAVEFORMATEXTENSIBLE*)pwfx)->dwChannelMask = originalChannelMask;
						hr = (*pAudioClient)->Initialize(
							AUDCLNT_SHAREMODE_SHARED,
							AUDCLNT_STREAMFLAGS_LOOPBACK,
							requestedBufferSizes[i],
							0,
							pwfx,
							nullptr);
					}

					// Valid format, quit searching.
					if (SUCCEEDED(hr))
					{
						hasFoundFormat = true;
						break;
					}
				}

				if (FAILED(hr))
				{
					WPX_Error("Failed overriding format with %i channels: block align %i, sample rate %i\n",
						channelCount, pwfx->nBlockAlign, pwfx->nSamplesPerSec);
				}
			}
		}

		if (FAILED(hr))
		{
			WINASSERT(!hasFoundFormat);
			goto ExitWASAPIInit;
		}
		WINASSERT(hasFoundFormat);
#else
		bool hasFoundFormat = false;
		while (pwfx->nChannels >= 1)
		{
			for (int i = 0; i < ARRAYSIZE(requestedBufferSizes); ++i)
			{
				hr = (*pAudioClient)->Initialize(
					AUDCLNT_SHAREMODE_SHARED,
					isLoopbackDevice ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
					requestedBufferSizes[i],
					0,
					pwfx,
					nullptr);
				HANDLE_WASAPI_INIT_ERROR_NOEXIT(hr, "(*pAudioClient)->Initialize failed.");

				// Print format details if we failed.
				if (FAILED(hr))
				{
					WPX_Error("Failed format: channels %i, block align %i, sample rate %i\n", pwfx->nChannels, pwfx->nBlockAlign, pwfx->nSamplesPerSec);
				}

				// Valid format, quit searching.
				if (SUCCEEDED(hr))
				{
					hasFoundFormat = true;
					break;
				}
			}

			if (hasFoundFormat)
			{
				break;
			}

			// All formats failed! Decrement channels artifically and hope for a match.
			--pwfx->nChannels;
			pwfx->nBlockAlign = 4 * pwfx->nChannels;
			channelCount = pwfx->nChannels;
		}

		if (FAILED(hr))
		{
			WINASSERT(!hasFoundFormat);
			goto ExitWASAPIInit;
		}
		WINASSERT(hasFoundFormat);
#endif

		// Events don't work with loopback
		//audioAvailabilityEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		//ResetEvent(audioAvailabilityEvent);
		//hr = pAudioClient->SetEventHandle(audioAvailabilityEvent);
		//EXIT_ON_ERROR(hr);

		// Get the size of the allocated buffer.
		//hr = (*pAudioClient)->GetBufferSize(&bufferFrameCount);
		//HANDLE_WASAPI_INIT_ERROR(hr, "(*pAudioClient)->GetBufferSize failed");

		hr = (*pAudioClient)->GetService(
			_IID_IAudioCaptureClient,
			(void**)pCaptureClient);
		HANDLE_WASAPI_INIT_ERROR(hr, "(*pAudioClient)->GetService failed");

		// Notify the audio sink which format to use.
		//hr = pMySink->SetFormat(pwfx);
		//EXIT_ON_ERROR(hr)

		// Calculate the actual duration of the allocated buffer.
		//hnsActualDuration = (double) REFTIMES_PER_SEC *
		//	bufferFrameCount / pwfx->nSamplesPerSec;

		hr = (*pAudioClient)->Start();  // Start recording.
		HANDLE_WASAPI_INIT_ERROR(hr, "(*pAudioClient)->Start failed");

		success = true;
		WPX_Msg("Successfully initialized WASAPI with %i channels.\n", channelCount);

		if (!isLoopbackDevice)
		{
			//deviceFlags |= AudioBufferFlags::AnalogDevice;
		}

	ExitWASAPIInit:
		if (!success)
		{
			SafeRelease(pCaptureClient);
			SafeRelease(pAudioClient);
			SafeRelease(pEnumerator);
		}
		SafeRelease(&pDevice);
		CoTaskMemFree(pwfx);
		//CoUninitialize();

		//SetThreadExecutionState(0);

		return success;
	}

	void StopWASAPI(IMMDeviceEnumerator **pEnumerator, IAudioClient **pAudioClient, IAudioCaptureClient **pCaptureClient)
	{
		if (*pAudioClient != nullptr)
			(*pAudioClient)->Stop();

		SafeRelease(pCaptureClient);
		SafeRelease(pAudioClient);
		//SAFE_RELEASE(*pEnumerator);
	}

	struct  WASAPIEventBase : public IMMNotificationClient
	{
	};

	struct  WASAPIEventImpl : public WASAPIEventBase
	{
		ULONG *refCount;
		function<void()> callback;

	public:
		WASAPIEventImpl(function<void()> callback)
			: refCount(nullptr)
			, callback(callback)
		{
			refCount = (ULONG*)_aligned_malloc(sizeof(ULONG), 32);
			memset(refCount, 0, sizeof(ULONG));
			InterlockedIncrement(refCount);
		}
		// IUnknown impl
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
		{
			return E_FAIL;
		}

		virtual ULONG STDMETHODCALLTYPE AddRef(void)
		{
			return InterlockedIncrement(refCount);
		}

		virtual ULONG STDMETHODCALLTYPE Release(void)
		{
			ULONG ref = InterlockedDecrement(refCount);
			if (ref == 0)
			{
				_aligned_free(refCount);
				delete this;
			}
			return ref;
		}

		// IMMNotificationClient impl
		virtual HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(_In_ LPCWSTR pwstrDeviceId, _In_ DWORD dwNewState) override
		{
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE OnDeviceAdded(_In_ LPCWSTR pwstrDeviceId) override
		{
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE OnDeviceRemoved(_In_ LPCWSTR pwstrDeviceId) override
		{
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(_In_ EDataFlow flow, _In_ ERole role, _In_ LPCWSTR pwstrDefaultDeviceId) override
		{
			callback();
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(_In_ LPCWSTR pwstrDeviceId, _In_ const PROPERTYKEY key) override
		{
			return S_OK;
		}
	};

	// Omitted from Windows header
	// https://msdn.microsoft.com/en-us/library/windows/desktop/aa373217(v=vs.85).aspx
	//typedef struct _SYSTEM_POWER_INFORMATION
	//{
	//	ULONG MaxIdlenessAllowed;
	//	ULONG Idleness;
	//	ULONG TimeRemaining;
	//	UCHAR CoolingMode;
	//} SYSTEM_POWER_INFORMATION, *PSYSTEM_POWER_INFORMATION;
}

AudioEventHandler::AudioEventHandler()
{
}

AudioEventHandler::~AudioEventHandler()
{
	WINASSERT(pEnumerator == nullptr);
}

void AudioEventHandler::Shutdown()
{
	if (pEnumerator != nullptr)
	{
		pEnumerator->UnregisterEndpointNotificationCallback(eventBase);
	}

	SafeRelease(&eventBase);
	SafeRelease(&pEnumerator);
}

AUDIO_EVENT_HANDLER_TOKEN AudioEventHandler::AddDefaultOutputChangedCallback(std::function<void()> callback)
{
	if (callbacks.empty() && pEnumerator == nullptr)
	{
		HRESULT hr = CoCreateInstance(
			_CLSID_MMDeviceEnumerator, nullptr,
			CLSCTX_ALL, _IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);

		if (SUCCEEDED(hr))
		{
			eventBase = new WASAPIEventImpl(bind(&AudioEventHandler::OnDefaultDeviceChanged, this));
		}

		pEnumerator->RegisterEndpointNotificationCallback(eventBase);
	}

	unique_lock<recursive_mutex> lock(callbacksMutex);
	AUDIO_EVENT_HANDLER_TOKEN token = ++nextCallback;
	callbacks[token] = callback;
	return token;
}

void AudioEventHandler::RemoveDefaultOutputChangedCallback(AUDIO_EVENT_HANDLER_TOKEN token)
{
	unique_lock<recursive_mutex> lock(callbacksMutex);
	callbacks.erase(token);
}

void AudioEventHandler::OnDefaultDeviceChanged()
{
	unique_lock<recursive_mutex> lock(callbacksMutex);
	for (auto itr : callbacks)
	{
		itr.second();
	}
}

AudioProcessor::AudioProcessor()
	: outputBuffer((float*)_mm_malloc(sizeof(float) * MAX_BARS * 2, 16))
	, deviceFlags(0)
	, volume(1.0f)
	, threshold(0.0f)
	, isRecording(false)
	, pAudioClient(nullptr)
	, pCaptureClient(nullptr)
	, pEnumerator(nullptr)
	, channelCount(0)
	, eventBase(new WASAPIEventImpl(bind(&AudioProcessor::OnDefaultDeviceChanged, this)))
	, defaultDeviceWasChanged(false)
	, numFramesFFT(0)
	, numBinsFFT(0)
	, sampleSleepTimer(33)
	//, powerProfModule(nullptr)
	//, pFnCallNtPowerInformation(nullptr)
	//, audioAvailabilityEvent(nullptr)
{
	ZeroMemory(outputBuffer, sizeof(float) * MAX_BARS * 2);
}

AudioProcessor::~AudioProcessor()
{
	SafeRelease(&pEnumerator);

	WINASSERT(!isRecording);
	WINASSERT(pAudioClient == nullptr);
	WINASSERT(pCaptureClient == nullptr);
	WINASSERT(!audioSampleThread.joinable());
	eventBase->Release();

	_mm_free(outputBuffer);
	//WINASSERT(audioAvailabilityEvent == nullptr);

	//if (powerProfModule != nullptr)
	//	FreeLibrary(powerProfModule);
}

int AudioProcessor::GetDefaultDeviceIndex()
{
	return 0;
}

void AudioProcessor::StartRecording(const string &deviceId)
{
	WINASSERT(!isRecording);
	WINASSERT(pAudioClient == nullptr);
	WINASSERT(pCaptureClient == nullptr);
	WINASSERT(!audioSampleThread.joinable());
	//WINASSERT(audioAvailabilityEvent == nullptr);

	//if (powerProfModule == nullptr)
	//{
	//	powerProfModule = LoadLibraryA("PowrProf");
	//	if (powerProfModule != nullptr)
	//	{
	//		pFnCallNtPowerInformation = (FnCallNtPowerInformation) GetProcAddress(powerProfModule, "CallNtPowerInformation");
	//	}
	//}

	//WINASSERT(powerProfModule != nullptr);
	//WINASSERT(pFnCallNtPowerInformation != nullptr);

	isRecording = StartWASAPI(audioProcessingParams, deviceId, channelCount, deviceFlags, numFramesFFT, numBinsFFT, &pEnumerator, &pAudioClient, &pCaptureClient);

	//sampleFrameCount = pwfx->nChannels 

	//fftConfig = kiss_fft_alloc(sampleFrameCount, 0, 0, 0);
	if (isRecording)
	{
		pEnumerator->RegisterEndpointNotificationCallback(eventBase);
		currentDevice = deviceId;
		audioSampleThread = thread(&AudioProcessor::SampleThread, this);
	}
}

void AudioProcessor::StopRecording()
{
	if (isRecording)
	{
		isRecording = false;
		if (pAudioClient != nullptr)
		{
			// Wait for sample thread to exit
			if (audioSampleThread.joinable())
			{
				// Tell sample thread to quit
				audioSampleThreadStopCondition.lock();
				// Wake up the thread
				//SetEvent(audioAvailabilityEvent);

				audioSampleThread.join();
				audioSampleThreadStopCondition.unlock();
			}

			if (pEnumerator != nullptr)
			{
				pEnumerator->UnregisterEndpointNotificationCallback(eventBase);
			}
			StopWASAPI(&pEnumerator, &pAudioClient, &pCaptureClient);
		}
	}

	//if (audioAvailabilityEvent != nullptr)
	//{
	//	CloseHandle(audioAvailabilityEvent);
	//	audioAvailabilityEvent = nullptr;
	//}

	WINASSERT(!audioSampleThread.joinable());
}

void AudioProcessor::SetMaxFPS(int maxFPS)
{
	sampleSleepTimer = CLAMP(floor(1000.0f / maxFPS) / 2, 6, 100);
}

void AudioProcessor::ReadAudioBufferStereo64(float *bars, int &audioBufferFlags)
{
	//WINASSERT(count <= MAX_BARS);
	//WINASSERT(channel >= 0 && channel <= 1);

	if (isRecording)
	{
		outputBufferMutex.lock();
		//ZeroMemory(bars, count * sizeof(float));
		//for (int i = 0; i < MAX_BARS; ++i)
		//{
		//	const int mapping = (float(i) / MAX_BARS) * count;
		//	const float value = outputBuffer[channel][i];
		//	bars[mapping] = bars[mapping] > value ? bars[mapping] : value;
		//}
		memcpy(bars, outputBuffer, sizeof(float) * MAX_BARS * 2);
		outputBufferMutex.unlock();

		audioBufferFlags = deviceFlags;
	}
	else
	{
		ZeroMemory(bars, MAX_BARS * 2 * sizeof(float));
		audioBufferFlags = 0;
	}
}

void AudioProcessor::OnDefaultDeviceChanged()
{
	defaultDeviceWasChanged = true;
}

void AudioProcessor::SampleThread()
{
	// http://stackoverflow.com/questions/641935/wasapi-prevents-windows-automatic-suspend

	CoInitializeEx(nullptr, COM_SHARED_STA | COINIT_DISABLE_OLE1DDE);

	HRESULT hr = S_OK;
	uint32 numFramesAvailable = 0;
	uint32 packetLength = 0;
	BYTE *pData = nullptr;
	DWORD flags = 0;

	// FFTS
	float *fin[2] = { nullptr };
	float *fout[2] = { nullptr };
	ffts_plan_t *fftsConfig = nullptr;

	//const uint32 numFramesFFT = MAX_BARS * 40;
	//const uint32 numBinsFFT = numFramesFFT / 8;
	uint32 numFramesFFTLast = 0;
	uint32 numBinsFFTLast = 0;
	uint32 numFramesFFTWriteIndex = 0;

	bool shouldRestart = false;
	bool isSilent = false;
	float silentTimer = 0.0f;

	BYTE *oldDataCache = nullptr;
	int oldDataCount = 0;

WasapiSampleRestart:

	while (!shouldRestart)
	{
		// Can't use event based processing on loopback devices.
		//Sleep(sampleSleepTimer);
		Sleep(1);

		// The signal to exit
		if (!audioSampleThreadStopCondition.try_lock())
		{
			break;
		}
		audioSampleThreadStopCondition.unlock();

		if (defaultDeviceWasChanged)
		{
			defaultDeviceWasChanged = false;
			shouldRestart = true;
			continue;
		}

		// Close the stream if we are supposed to sleep now.
		//if (powerCheckTimer > 0.0f)
		//{
		//	// Match sleep time
		//	powerCheckTimer -= 0.015f;

		//	if (powerCheckTimer <= 0.0f)
		//	{
		//		powerCheckTimer = 3.0f;
		//		if (pFnCallNtPowerInformation != nullptr)
		//		{
		//			SYSTEM_POWER_INFORMATION spi = { 0 };
		//			if (pFnCallNtPowerInformation(SystemPowerInformation, NULL, 0,
		//				&spi, sizeof(spi)) >= 0)
		//			{
		//				WPX_Msg("Time remaining: %u\n", spi.TimeRemaining);
		//				if (spi.TimeRemaining == 0)
		//				{
		//					shouldRestart = true;
		//					continue;
		//				}
		//			}
		//		}
		//	}
		//}

		if (pCaptureClient == nullptr)
		{
			StartWASAPI(audioProcessingParams, currentDevice, channelCount, deviceFlags, numFramesFFT, numBinsFFT, &pEnumerator, &pAudioClient, &pCaptureClient);
			continue;
		}

		const int numFramesNotZeroPadded = numFramesFFT - numFramesFFT * (audioProcessingParams.frequencyDomainSizeScale / audioProcessingParams.timeDomainSizeScale);

		const float mappingRange = 127.0f;
		if (numFramesFFT != numFramesFFTLast || numBinsFFT != numBinsFFTLast)
		{
			numFramesFFTLast = numFramesFFT;
			numBinsFFTLast = numBinsFFT;
			numFramesFFTWriteIndex = 0;

			if (fftsConfig)
				ffts_free(fftsConfig);
			fftsConfig = ffts_init_1d(numFramesFFT, FFTS_FORWARD);

			if (fin[0] != nullptr)
				_mm_free(fin[0]);
			if (fin[1] != nullptr)
				_mm_free(fin[1]);

			if (fout[0] != nullptr)
				_mm_free(fout[0]);
			if (fout[1] != nullptr)
				_mm_free(fout[1]);

			fin[0] = (float*)_mm_malloc(2 * numFramesFFT * sizeof(float), 32);
			fout[0] = (float*)_mm_malloc(2 * numFramesFFT * sizeof(float), 32);
			fin[1] = (float*)_mm_malloc(2 * numFramesFFT * sizeof(float), 32);
			fout[1] = (float*)_mm_malloc(2 * numFramesFFT * sizeof(float), 32);

			for (int i = 0; i < numFramesFFT; ++i)
			{
				fin[0][i * 2] = mappingRange;
				fin[0][i * 2 + 1] = 1.0f / fin[0][i * 2];

				fin[1][i * 2] = mappingRange;
				fin[1][i * 2 + 1] = 1.0f / fin[1][i * 2];
			}
		}

		packetLength = 0;
		hr = pCaptureClient->GetNextPacketSize(&packetLength);
		HANDLE_WASAPI_DEVICE_SAMPLE_ERROR(hr, "pCaptureClient->GetNextPacketSize failed");

		bool staySilent = false;
		if (packetLength == 0)
		{
			if (silentTimer > 1000.0f)
				isSilent = true;
			else
				silentTimer += sampleSleepTimer;
		}
		else
		{
			silentTimer = 0.0f;
		}

		while (hr == S_OK && packetLength != 0 && !shouldRestart)
		{
			// Get the available data in the shared buffer.
			flags = 0;
			hr = pCaptureClient->GetBuffer(
				&pData,
				&numFramesAvailable,
				&flags, nullptr, nullptr);
			HANDLE_WASAPI_DEVICE_SAMPLE_ERROR(hr, "pCaptureClient->GetBuffer failed");

			//WPX_Msg("Sampled frame count: %i\n", numFramesAvailable);

			isSilent = staySilent || (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || hr != S_OK;
			if (isSilent)
			{
				pData = nullptr;  // Write silence
			}

			//WPX_Msg("frames %i\n", numFramesAvailable);

			// Accumulate samples for FFT
			if (pData != nullptr && numFramesFFTWriteIndex < numFramesFFT)
			{
				if (oldDataCount > 0)
				{
					const int cacheEndWriteIndex = MIN(numFramesNotZeroPadded, numFramesFFTWriteIndex + oldDataCount);
					if (channelCount >= 2)
					{
						for (int i = numFramesFFTWriteIndex; i < cacheEndWriteIndex; ++i)
						{
							const int readIndex = i - numFramesFFTWriteIndex;
							fin[0][i * 2] = *reinterpret_cast<float*>(&oldDataCache[readIndex * 4 * channelCount]);
							fin[0][i * 2] = (fin[0][i * 2] * mappingRange) + mappingRange;
							fin[0][i * 2 + 1] = 1.0f / fin[0][i * 2];

							fin[1][i * 2] = *reinterpret_cast<float*>(&oldDataCache[readIndex * 4 * channelCount + 4]);
							fin[1][i * 2] = (fin[1][i * 2] * mappingRange) + mappingRange;
							fin[1][i * 2 + 1] = 1.0f / fin[1][i * 2];
						}
					}
					else
					{
						for (int i = numFramesFFTWriteIndex; i < cacheEndWriteIndex; ++i)
						{
							const int readIndex = i - numFramesFFTWriteIndex;
							fin[0][i * 2] = *reinterpret_cast<float*>(&oldDataCache[readIndex * 4 * channelCount]);
							fin[0][i * 2] = (fin[0][i * 2] * mappingRange) + mappingRange;
							fin[0][i * 2 + 1] = 1.0f / fin[0][i * 2];
						}
					}
					numFramesFFTWriteIndex = cacheEndWriteIndex;

					oldDataCount = 0;
				}

				const int framesSkipped = MAX(0, numFramesFFTWriteIndex + numFramesAvailable - numFramesNotZeroPadded);
				const int endWriteIndex = MIN(numFramesNotZeroPadded, numFramesFFTWriteIndex + numFramesAvailable);
				if (channelCount >= 2)
				{
					for (int i = numFramesFFTWriteIndex; i < endWriteIndex; ++i)
					{
						const int readIndex = i - numFramesFFTWriteIndex;
						fin[0][i * 2] = *reinterpret_cast<float*>(&pData[readIndex * 4 * channelCount]);
						fin[0][i * 2] = (fin[0][i * 2] * mappingRange) + mappingRange;
						fin[0][i * 2 + 1] = 1.0f / fin[0][i * 2];

						fin[1][i * 2] = *reinterpret_cast<float*>(&pData[readIndex * 4 * channelCount + 4]);
						fin[1][i * 2] = (fin[1][i * 2] * mappingRange) + mappingRange;
						fin[1][i * 2 + 1] = 1.0f / fin[1][i * 2];
					}
				}
				else
				{
					for (int i = numFramesFFTWriteIndex; i < endWriteIndex; ++i)
					{
						const int readIndex = i - numFramesFFTWriteIndex;
						fin[0][i * 2] = *reinterpret_cast<float*>(&pData[readIndex * 4 * channelCount]);
						fin[0][i * 2] = (fin[0][i * 2] * mappingRange) + mappingRange;
						fin[0][i * 2 + 1] = 1.0f / fin[0][i * 2];
					}
				}

				// Finished writing data, store remaining data for next calculations
				if (endWriteIndex == numFramesFFTWriteIndex)
				{
					oldDataCount = framesSkipped;
					if (oldDataCount > 0)
					{
						delete[] oldDataCache;
						oldDataCache = nullptr;
						oldDataCache = new BYTE[oldDataCount * 4 * channelCount];
						memcpy(oldDataCache,
							pData + (numFramesAvailable - framesSkipped) * 4 * channelCount,
							framesSkipped * 4 * channelCount);
					}
				}

				if (threshold > FLT_EPSILON &&
					numFramesFFTWriteIndex < endWriteIndex)
				{
					float maxIn = 0.0f;
					for (int i = numFramesFFTWriteIndex; i < endWriteIndex; ++i)
					{
						const int readIndex = i - numFramesFFTWriteIndex;
						float v = *reinterpret_cast<float*>(&pData[readIndex * 4 * channelCount]);
						maxIn = MAX(maxIn, v);
					}

					if (maxIn < threshold)
					{
						//	WPX_Msg("pause audio: %f\n", maxIn);
						isSilent = staySilent = true;
					}
					//else
					//	WPX_Msg("record audio: %f\n", maxIn);
				}

				numFramesFFTWriteIndex = endWriteIndex;
			}

			hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
			HANDLE_WASAPI_DEVICE_SAMPLE_ERROR(hr, "pCaptureClient->ReleaseBuffer failed");

			hr = pCaptureClient->GetNextPacketSize(&packetLength);
			HANDLE_WASAPI_DEVICE_SAMPLE_ERROR(hr, "pCaptureClient->GetNextPacketSize failed");
		}

		bool shouldRunCallback = false;
		if (isSilent || shouldRestart)
		{
			Sleep(sampleSleepTimer);
			numFramesFFTWriteIndex = 0;
			outputBufferMutex.lock();
			ZeroMemory(outputBuffer, sizeof(float) * MAX_BARS * 2);
			outputBufferMutex.unlock();

			shouldRunCallback = true;
		}
		// FFT input buffer full
		else if (numFramesFFTWriteIndex == numFramesNotZeroPadded)
		{
			numFramesFFTWriteIndex = 0;

			// Run FFT
			if (channelCount >= 2)
			{
				ffts_execute(fftsConfig, fin[0], fout[0]);
				ffts_execute(fftsConfig, fin[1], fout[1]);
			}
			else
			{
				ffts_execute(fftsConfig, fin[0], fout[0]);
			}

			// Clear bars
			ALIGN16_PREFIX float localBuffer[2][MAX_BARS] ALIGN16_SUFFIX = { 0 };
			const int bucketSkips = 1;
			//int mustSkipOne = 0;
			//const float piFactorA = M_PI_F * 0.1f;
			//const float piFactorB = M_PI_F * 0.9f;
			const float windowBinsInv = 1.0f / (numBinsFFT - bucketSkips);
			const float hammingParam = audioProcessingParams.hammingParam;

			for (int c = 0; c < MIN(2, channelCount); ++c)
			{
				float *out = fout[c];
				//kiss_fft_cpx *cOut = out[c];
				float *cLocalBuffer = localBuffer[c];
				int lastBucket = 0;

				for (int i = bucketSkips; i < numBinsFFT; ++i)
				{
					float p = out[i * 2] * out[i * 2] + out[i * 2 + 1] * out[i * 2 + 1];
					//p = MAX(0.0f, p);
					if (!isfinite(p))
					{
						p = 0.0f;
					}

					//const float windowBins = (numBinsFFT - bucketSkips) * 2;
					//const float currentBin = i - bucketSkips + numBinsFFT - bucketSkips;
					const float currentBin = i - bucketSkips;

					// Hamming window
					p *= hammingParam - (1.0f - hammingParam) * cos(M_PI_F * currentBin * windowBinsInv);
					//p *= hammingParam - (1.0f - hammingParam) * cos(M_PI_F * currentBin * windowBinsInv);
					//p *= hammingParam - (1.0f - hammingParam) * cos(piFactorA + piFactorB * currentBin * windowBinsInv);
					//p *= (hammingParam - (1.0f - hammingParam) * cos(M_PI_F2 * currentBin / (windowBins - 1))) * 0.25f + 0.75f;


					// Blackmann
					//const float a0 = 0.42,
					//	a1 = 0.5,
					//	a2 = 0.08,
					//	f = 6.283185307179586*currentBin / (windowBins - 1.0f);
					//p *= a0 - a1 * cos(f) + a2 * cos(2 * f);

					// Blackmann harris
					//const float a0 = 0.35875,
					//	a1 = 0.48829,
					//	a2 = 0.14128,
					//	a3 = 0.01168,
					//	f = 6.283185307179586*currentBin / (windowBins - 1);
					//p *= a0 - a1 * cos(f) + a2 * cos(2 * f) - a3 * cos(3 * f);


					float m = sqrt(p);

					float mapping = (i - bucketSkips) / float(numBinsFFT - bucketSkips);
					int b = powf(mapping, audioProcessingParams.spreadPower) * (MAX_BARS);
					b %= MAX_BARS;

					//if (mustSkipOne == 1 && b == lastBucket + 1)
					//{
					//	++b;
					//	mustSkipOne = 2;
					//}

					if (b > lastBucket + 1)
						b = lastBucket + 1;

					//if (b > lastBucket + 1)
					//{
					//	mustSkipOne = mustSkipOne == 2 ? 0 : 1;
					//	const float last = cLocalBuffer[lastBucket];
					//	const float end = m;

					//	const int indexStart = lastBucket + 1;
					//	for (int l = indexStart; l < b; ++l)
					//	{
					//		float perc = (l - indexStart + 1) / float(b - indexStart + 1);
					//		perc = perc * perc * (3.0f - 2.0f * perc);
					//		cLocalBuffer[l] = last + (end - last) * perc;
					//	}

					//	//b = lastBucket + 1;
					//}
					lastBucket = b;

					cLocalBuffer[b] = cLocalBuffer[b] > m ? cLocalBuffer[b] : m;
				}

				//cLocalBuffer[1] = (cLocalBuffer[2] + cLocalBuffer[3]) * 0.25f;
				//cLocalBuffer[0] = cLocalBuffer[1] * 0.5f;
			}

			const float volumeScale = numBinsFFT / (numFramesFFT * 0.5f);

			// Normalize bars
			for (int i = 0; i < MAX_BARS; ++i)
			{
				localBuffer[0][i] *= BAR_VOLUME_SCALE * volume * volumeScale;
				localBuffer[1][i] *= BAR_VOLUME_SCALE * volume * volumeScale;
			}

			outputBufferMutex.lock();
#if defined(DEBUG_PC) && 0
			for (int c = 0; c < 2; ++c)
			{
				for (int i = 0; i < MAX_BARS; ++i)
				{
					outputBuffer[c][i] += (localBuffer[c][i] - outputBuffer[c][i]) * 0.5f;
				}
			}
#else
			memcpy(outputBuffer, localBuffer[0], sizeof(localBuffer[0]));
			memcpy(outputBuffer + MAX_BARS, localBuffer[channelCount >= 2 ? 1 : 0], sizeof(localBuffer[0]));
#endif
			outputBufferMutex.unlock();

			shouldRunCallback = true;
		}

		if (shouldRunCallback && callback != nullptr)
		{
			if (isRecording)
			{
				callback(outputBuffer);
			}
			else
			{
				float silentData[MAX_BARS * 2] = { 0 };
				callback(silentData);
			}
		}
	}

	if (shouldRestart)
	{
		//bool isWASAPIAllowed = true;
		//if (pFnCallNtPowerInformation != nullptr)
		//{
		//	SYSTEM_POWER_INFORMATION spi = { 0 };
		//	if (pFnCallNtPowerInformation(SystemPowerInformation, NULL, 0,
		//		&spi, sizeof(spi)) >= 0)
		//	{
		//		if (spi.TimeRemaining == 0)
		//		{
		//			isWASAPIAllowed = false;
		//		}
		//	}
		//}

		StopWASAPI(&pEnumerator, &pAudioClient, &pCaptureClient);
		//if (isWASAPIAllowed)
		{
			shouldRestart = false;
			StartWASAPI(audioProcessingParams, currentDevice, channelCount, deviceFlags, numFramesFFT, numBinsFFT, &pEnumerator, &pAudioClient, &pCaptureClient);
		}

		goto WasapiSampleRestart;
	}

	delete[] oldDataCache;

	// FFTS
	if (fftsConfig)
		ffts_free(fftsConfig);

	if (fin[0] != nullptr)
		_mm_free(fin[0]);
	if (fin[1] != nullptr)
		_mm_free(fin[1]);

	if (fout[0] != nullptr)
		_mm_free(fout[0]);
	if (fout[1] != nullptr)
		_mm_free(fout[1]);

	CoUninitialize();
}
