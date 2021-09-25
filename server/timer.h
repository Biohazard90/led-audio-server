
#pragma once

#ifdef PLATFORM_WINDOWS
class Timer
{
public:
    Timer()
    {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t);
    }

    inline float Update()
    {
        LARGE_INTEGER t2;
        QueryPerformanceCounter(&t2);
        float dt = (t2.QuadPart - t.QuadPart) / float(freq.QuadPart);
        t = t2;
        return dt;
    }

    //inline float UpdateFromReferenceTime(LARGE_INTEGER &tExt)
    //{
    //	float dt = (tExt.QuadPart - t.QuadPart) / float(freq.QuadPart);
    //	t = tExt;
    //	return dt;
    //}

    inline void UpdateReferenceTime()
    {
        QueryPerformanceCounter(&t);
    }

    LARGE_INTEGER t;
    LARGE_INTEGER freq;
};

class PersistentTimer
{
public:
    PersistentTimer()
    {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t);
    }

    inline float Update()
    {
        LARGE_INTEGER t2;
        QueryPerformanceCounter(&t2);
        float dt = (t2.QuadPart - t.QuadPart) / float(freq.QuadPart);
        t = t2;
        timePassed += dt;
        return dt;
    }

    inline void UpdateReferenceTime()
    {
        QueryPerformanceCounter(&t);
    }

    inline void Reset()
    {
        timePassed = 0.0f;
    }

    inline float GetTimePassed() const
    {
        return timePassed;
    }

    LARGE_INTEGER t;
    LARGE_INTEGER freq;
    float timePassed = 0.0f;
};
#else

#include <time.h>

#define __MAX_GETTIME_MULT_SEC 1000000000ull
#define __MAX_GETTIME_DIV_SEC  1000000000.0

class Timer
{
public:
    Timer()
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    }

    inline float Update()
    {
        timespec t2;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
        const float dt = double(uint64(t2.tv_sec - t.tv_sec) * __MAX_GETTIME_MULT_SEC + t2.tv_nsec - t.tv_nsec) / __MAX_GETTIME_DIV_SEC;
        t = t2;
        return dt;
    }

    inline void UpdateReferenceTime()
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    }

    timespec t;
};

class PersistentTimer
{
public:
    PersistentTimer()
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    }

    inline float Update()
    {
        timespec t2;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
        const float dt = double(uint64(t2.tv_sec - t.tv_sec) * __MAX_GETTIME_MULT_SEC + t2.tv_nsec - t.tv_nsec) / __MAX_GETTIME_DIV_SEC;
        t = t2;
        timePassed += dt;
        return dt;
    }

    inline void UpdateReferenceTime()
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    }

    inline void Reset()
    {
        timePassed = 0.0f;
    }

    inline float GetTimePassed() const
    {
        return timePassed;
    }

    timespec t;
    float timePassed = 0.0f;
};

#endif
