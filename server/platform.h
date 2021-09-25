#pragma once

#ifdef PLATFORM_WINDOWS
#ifndef PLATFORM_WINDOWS_RT
#define WIN32_LEAN_AND_MEAN
#endif
#include "stdint.h"
#define VC_EXTRALEAN
#elif PLATFORM_ANDROID
#include "stdint.h"
#include "stdlib.h"
#else
#error missing platform
#endif

#ifndef int8
typedef int8_t int8;
#endif

#ifndef uint8
typedef uint8_t uint8;
#endif

#ifndef int16
typedef int16_t int16;
#endif

#ifndef uint16
typedef uint16_t uint16;
#endif

#ifndef uint32
typedef uint32_t uint32;
#endif

#ifndef int64
typedef int64_t int64;
#endif

#ifndef uint64
typedef uint64_t uint64;
#endif

#ifdef PLATFORM_X64
typedef uint64_t uptr;
#else
typedef uint32_t uptr;
#endif

extern void WPX_FatalError(const char *format, ...);
extern void WPX_Error(const char *format, ...);
extern void WPX_ErrorFlags(const char *format, int flags, ...);
extern void WPX_Msg(const char *format, ...);

#if 0
#define FILTEREDMSG(f, ...) WPX_Msg(f, __VA_ARGS__)
#else
#define FILTEREDMSG(f, ...)
#endif

#include <algorithm>
#include <atomic>
#include <codecvt>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef PLATFORM_WINDOWS_RT
#include <d2d1_1.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <Dwrite.h>

#include <d2d1effects.h>
#include <DispatcherQueue.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <winrt/windows.foundation.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Core.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.Effects.h>
#include <winrt/Windows.UI.Composition.h>
#include <Windows.UI.Composition.Interop.h>

#include "shtypes.h"
#include "shlobj_core.h"
#include "shlwapi.h"
#endif

#ifdef PLATFORM_WINDOWS

#include <fstream>
#include <windows.h>

#ifdef _DEBUG
#define DEBUG_PC
#endif // _DEBUG

#ifdef DEBUG_PC
#include <assert.h>
#define WINASSERT(x) assert(x)
#else // NOT DEBUG_PC
#define WINASSERT(x)
#endif // end DEBUG_PC

#define ASSERTJSON(x) \
	WINASSERT(x.type() >= 0 && x.type() <= Json::objectValue)

inline bool GetErrorMessage(HRESULT hr, std::wstring &message)
{
	LPWSTR errorMessage;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
		nullptr, hr, 0, (LPTSTR)&errorMessage, 0, nullptr); // MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
	if (errorMessage == nullptr)
	{
		return false;
	}
	else
	{
		message = errorMessage;
		LocalFree(errorMessage);
		return true;
	}
}

inline HMODULE LoadUserLibrary(const wchar_t *libraryName)
{
	// Try loading from standard path
	HMODULE libraryHandle = LoadLibrary(libraryName);

	// Try loading with user user paths enabled
	if (libraryHandle == nullptr)
	{
		libraryHandle = LoadLibraryEx(libraryName, nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	}

	// Load completely failed
	if (libraryHandle == nullptr)
	{
		const int lastError = GetLastError();
		WPX_Error("WPX_Error loading library %S (%i).\n", libraryName, lastError);
		WINASSERT(0);
	}

	return libraryHandle;
}

#define ALIGN16_PREFIX __declspec(align(16))
#define ALIGN16_SUFFIX
#define NOINLINE __declspec(noinline)

enum
{
	WM_TRAY = WM_USER,
	WM_USER_ON_IPC_MESSAGE,
	WM_USER_KONAMI_UNLOCK, // Cross-process
	WM_USER_DROP_SHADOW_WINDOW_INIT,
	WM_USER_APPINJECT_COMMAND,
	WM_USER_SHOW_CURSOR,
	WM_USER_SHOW_ICONS,
	WM_USER_BALLOON_ERROR,
	WM_USER_ON_SCREEN_LOCK,
	WM_USER_TRIGGER_DELAYED_RESOLUTION_CHANGED,

	WM_USER_SET_VIDEO,
	WM_USER_VIDEO_ENDED,
	WM_USER_VIDEO_SETTINGS_CHANGED,
	WM_USER_WALLPAPER_SET_PAUSED,
	WM_USER_WALLPAPER_SET_MUTED,
	WM_USER_WALLPAPER_REFRESH_LAYOUT,
	WM_USER_WALLPAPER_LOAD_PLUGINS,

	WM_USER_PLUGINS_LOADED,
	//WM_USER_RESET_UI_WINDOW,

	WM_USER_APPLICATION_PURCHASED,
	WM_USER_RELOAD_TRAY_ICON,
	WM_USER_RELOAD_SCREENSAVER_PREVIEW,
};

enum
{
	BALLOON_NOTICE_ADVERTISE_TRAY = 0,
	BALLOON_NOTICE_WORKER_ERROR,
	BALLOON_NOTICE_MEDIA_FOUNDATION_ERROR,
	BALLOON_NOTICE_APPLICATION_INJECT_ERROR,
	BALLOON_NOTICE_PKG_VERSION_ERROR,
	BALLOON_NOTICE_WALLPAPER_FILE_MISSING,
};

// Even though many COM interfaces used by WE should be used with MTA, this
// option must not be changed to MTA because that causes random problems
#define COM_SHARED_STA COINIT_APARTMENTTHREADED

#if REG_DWORD == REG_DWORD_LITTLE_ENDIAN
# define le_to_host_uint16(VAL) VAL
# define be_to_host_uint16(VAL) _byteswap_ushort(VAL)
# define le_to_host_uint32(VAL) VAL
# define be_to_host_uint32(VAL) _byteswap_ulong(VAL)
# define le_to_host_uint64(VAL) VAL
# define be_to_host_uint64(VAL) _byteswap_uint64(VAL)
#else
# define le_to_host_uint16(VAL) _byteswap_ushort(VAL)
# define be_to_host_uint16(VAL) VAL
# define le_to_host_uint32(VAL) _byteswap_ulong(VAL)
# define be_to_host_uint32(VAL) VAL
# define le_to_host_uint64(VAL) _byteswap_uint64(VAL)
# define be_to_host_uint64(VAL) VAL
#endif

#define WE_NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#define WE_NT_ERROR(status) (((NTSTATUS)(status)) < 0)
#define WE_NT_STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

#else // NOT PLATFORM_WINDOWS
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3ext.h>
#include <GLES3/gl32.h>
#include <cstring>
#include <cstdlib>

#ifndef NDEBUG
#define DEBUG_PC
#endif // _DEBUG

#ifdef DEBUG_PC
#include <assert.h>
#define WINASSERT(x) assert(x)
#else // NOT DEBUG_PC
#define WINASSERT(x)
#endif // end DEBUG_PC

#define sprintf_s sprintf

//#if defined(__arm__) || defined(__aarch64__)
//#ifdef DEBUG_PC
//inline void *_mm_malloc(int a, int b)
//{
//    void *r;
//    posix_memalign(&r, b, (a + (b - (a % b))));
//    WINASSERT(r);
//    return r;
//}
//#define _mm_free(a)      free(a)
//#else
//inline void *_mm_malloc(int a, int b)
//{
//    void *r;
//    posix_memalign(&r, b, (a + (b - (a % b))));
//    return r;
//}
//#define _mm_free(a)      free(a)
//#endif
//#endif

//#elif defined(__aarch64__)
//#ifdef DEBUG_PC
//inline void *_mm_malloc(int a, int b) { void *r = aligned_alloc(b, (a + (b - (a % b)))); WINASSERT(r); return r; }
//#define _mm_free(a)      free(a)
//#else
//#define _mm_malloc(a, b) aligned_alloc(b, (a + (b - (a % b))))
//#define _mm_free(a)      free(a)
//#endif
//#endif

#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _wcsicmp wcscasecmp

#define FORCEINLINE inline

#define ALIGN16_PREFIX
#define ALIGN16_SUFFIX __attribute__((aligned(16)))

#endif // end PLATFORM_WINDOWS

#define ASSERTALIGN16(x) WINASSERT(((uint64)(x) & 0xF) == 0)

#define WAITFORDEBUGGER() \
	{ \
		while ( !::IsDebuggerPresent() ) \
			::Sleep( 100 ); \
		::DebugBreak();  \
	}

#define CRASH() \
	{ \
		int *c = 0; *c = 0xDEADBEEF; \
	}

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#endif

#ifndef MIN
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#endif

#ifndef CLAMP
#define CLAMP(x, minValue, maxValue) (MAX(MIN(maxValue, x), minValue))
#endif

#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

#ifndef M_PI_F2
#define M_PI_F2 (3.14159265358979323846f * 2.0f)
#endif

#define MAX_PATH_APP 1024

#ifdef DEBUG_PC
#ifndef UNREACHABLE
#define UNREACHABLE() WINASSERT(0)
#endif // end UNREACHABLE
#else
#ifndef UNREACHABLE
#ifdef _MSC_VER
#define UNREACHABLE() __assume(0)
#else // _MSC_VER
#define UNREACHABLE() __builtin_unreachable()
#endif // end _MSC_VER
#endif // end UNREACHABLE
#endif

#define UNREACHABLE_DEFAULT() \
	default: UNREACHABLE();

#define UNREACHABLE_DEFAULT_SAFE() \
	default: WINASSERT(0)

#define VALIDATE_NOT_THREAD_SAFE() \
	{ \
		static DWORD initialThreadId = ::GetCurrentThreadId(); \
		WINASSERT(::GetCurrentThreadId() == initialThreadId); \
	}

#define UNIXTIMESTAMP() (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
#define UNIXTIMESTAMP_SECONDS() (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count())

#define __WIDEN(x) L ## x
#define WIDEN(x) __WIDEN(x)

#ifndef PLATFORM_WINDOWS_RT
template <class T>
void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = nullptr;
	}
}

inline void *CreateComBSTR(const std::wstring &ws)
{
	//uint8 *data = new uint8[4 + ws.size() * 2 + 2];
	//*(int*)data = ws.size() * 2;
	//memcpy(data + 4, ws.data(), ws.size() * 2);
	//memset(data + 4 + ws.size() * 2, 0, 2);

	uint8 *data = new uint8[ws.size() * 2 + 2];
	memcpy(data, ws.data(), ws.size() * 2);
	memset(data + ws.size() * 2, 0, 2);
	return data;
}

inline void ReleaseComBSTR(void *bstr)
{
	delete[]((uint8*)bstr);
}
#else
template <class T>
void SafeReleaseGeneric(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = nullptr;
	}
}
template <class T>
void SafeReleaseCom(T &ppT)
{
	if (ppT.get())
	{
		ppT->Release();
		ppT.detach();
	}
}
#endif

typedef std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> UTF32CONVERT;

#ifdef PLATFORM_WINDOWS
// DO NOT USE! CERTAIN CONVERSION ARE JUST WRONG BAD BAD BAD!!!
//typedef std::wstring_convert<std::codecvt_utf8<wchar_t>> UTF8CONVERT;

inline std::string utf8_encode(const std::wstring &wstr)
{
	if (!wstr.empty())
	{
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
		if (size_needed > 0)
		{
			std::string strTo(size_needed, 0);
			WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
			return strTo;
		}
	}
	return std::string();
}

// Convert an UTF8 string to a wide Unicode String
inline std::wstring utf8_decode(const std::string &str)
{
	if (!str.empty())
	{
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
		if (size_needed > 0)
		{
			std::wstring wstrTo(size_needed, 0);
			MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
			return wstrTo;
		}
	}
	return std::wstring();
}

#define TO_UTF8(x) utf8_encode(x)
#define FROM_UTF8(x) utf8_decode(x)
#endif
