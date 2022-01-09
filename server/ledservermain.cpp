
#include "platform.h"

#include "audioprocessor.h"
#include "SerialPort.hpp"
#include "timer.h"

#include <combaseapi.h>

using namespace std;

extern "C" __declspec(dllexport) DWORD NoHotPatch = 0x1;

void WPX_FatalError(const char *format, ...)
{
	char dest[4096];
	va_list argptr;
	va_start(argptr, format);
	vsprintf_s(dest, format, argptr);
	va_end(argptr);
	dest[4095] = 0;
	FatalAppExitA(0, dest);
}

void WPX_Error(const char *format, ...)
{
	char dest[8192 * 2];
	va_list argptr;
	va_start(argptr, format);
	vsprintf_s(dest, format, argptr);
	va_end(argptr);
	dest[8192 * 2 - 1] = 0;

#ifndef FINAL_BUILD
	OutputDebugStringA(dest);
#endif
}

void WPX_ErrorFlags(const char *format, int flags, ...)
{
	char dest[8192 * 2];
	va_list argptr;
	va_start(argptr, flags);
	vsprintf_s(dest, format, argptr);
	va_end(argptr);
	dest[8192 * 2 - 1] = 0;

#ifndef FINAL_BUILD
	OutputDebugStringA(dest);
#endif
}

void WPX_Msg(const char *format, ...)
{
	char dest[8192 * 2];
	va_list argptr;
	va_start(argptr, format);
	vsprintf_s(dest, format, argptr);
	va_end(argptr);
	dest[8192 * 2 - 1] = 0;

#ifndef FINAL_BUILD
	OutputDebugStringA(dest);
#endif
}

SerialPort *arduino;

bool IsStillConnected()
{
	return arduino != nullptr && arduino->isConnected();
}

bool TryConnectClient()
{
	if (arduino)
		delete arduino;
	arduino = nullptr;

	for (int i = 0; i < 64; ++i)
	{
		stringstream name;
		name << R"(\\.\COM)" << i;

		arduino = new SerialPort(name.str().c_str());
		if (arduino->isConnected())
		{
			return true;
		}
		else
		{
			delete arduino;
			arduino = nullptr;
		}
	}

	return false;
}

void WaitForClientConnection()
{
	while (!TryConnectClient())
	{
		Sleep(3000);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow)
{
#ifdef DEBUG_PC
	HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	CoInitializeEx(nullptr, COM_SHARED_STA | COINIT_DISABLE_OLE1DDE);

	WaitForClientConnection();

	Timer timer;
	float bufferCache[128] = { 0 };
	float normalizationMean = 0.5f;

	AudioProcessor audioProcessor;

	//static PersistentTimer fpsTimer;
	//static int frameCount = 0;

	float oldBuffer[2][128] = { 0 };

	audioProcessor.SetCallback([&timer, &normalizationMean, &bufferCache, &oldBuffer](float *buffer2x64) {
		if (!IsStillConnected())
		{
			if (!TryConnectClient())
				return;
		}

		float tmp[128] = { 0 };
		memcpy(tmp, buffer2x64, 4 * 128);

		for (int b = 0; b < 2; ++b)
		{
			for (int i = 0; i < 128; ++i)
			{
				buffer2x64[i] = MAX(buffer2x64[i], oldBuffer[b][i]);
			}
		}

		memcpy(oldBuffer[2], oldBuffer[1], 4 * 128);
		memcpy(oldBuffer[1], tmp, 4 * 128);

		float dt = timer.Update();
		dt = MIN(0.2f, dt);
		const int ledDataSize = 128;

		//float avgNoise = 0.0f;
		float maxNoise = 0.0f;
		for (int i = 0; i < 64; ++i)
		{
			//buffer2x64[i] *= 1 + 3 * (i + 1) / (1 + i * i);
			//buffer2x64[64 + i] *= 1 + 3 * (i + 1) / (1 + i * i);

			//avgNoise += buffer2x64[i] + buffer2x64[64 + i];
			maxNoise = MAX(maxNoise, MAX(buffer2x64[i], buffer2x64[64 + i]));
		}

		//avgNoise /= 128.0f;
		//const float noiseTriggerLevel = (maxNoise > 0.01f) ? avgNoise * 0.333f : 0.5f;
		const float noiseTriggerLevel = (maxNoise > 0.01f) ? maxNoise * 0.333f : 0.5f;
		normalizationMean += (noiseTriggerLevel - normalizationMean) * dt * 0.7f;

		for (int i = 0; i < ledDataSize; ++i)
		{
			//const float target = (buffer2x64[i] > normalizationMean * 1.5f) ? 1.0f : 0.0f;
			const float target = CLAMP((buffer2x64[i] - normalizationMean * 0.7f) / (normalizationMean * 0.6f), 0, 1);
			const float adjSpeed = target > 0.0f ? 18.0f : 16.0f;
			bufferCache[i] += (target - bufferCache[i]) * dt * adjSpeed;
			//bufferCache[i] = target;
		}

		uint8 ledData[ledDataSize];

		for (int i = 0; i < 64; ++i)
		{
			ledData[i] = CLAMP(bufferCache[64 + i] * 255.0f, 0, 255);
			ledData[64 + i] = CLAMP(bufferCache[i] * 255.0f, 0, 255);
		}

		arduino->writeSerialPort((const char*)ledData, ledDataSize);

		//++frameCount;
		//fpsTimer.Update();
		//if (fpsTimer.GetTimePassed() >= 1.0f)
		//{
		//	fpsTimer.Reset();
		//	WPX_Msg("audio FPS: %i\n", frameCount);
		//	frameCount = 0;
		//}
	});

	audioProcessor.StartRecording("");

	MSG msg = { 0 };
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	audioProcessor.StopRecording();

	delete arduino;

	CoUninitialize();
	return 0;
}
