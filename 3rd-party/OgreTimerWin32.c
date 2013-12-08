/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2013 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

// cy : original is c++, I modify it to c
#include "OgreTimerWin32.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#if OGRE_PLATFORM == OGRE_PLATFORM_WINRT
DWORD OgreTimerGetTickCount() { return (DWORD)GetTickCount64(); }
#endif

//-------------------------------------------------------------------------
void OgreTimerInit(struct OgreTimer *timer) {
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	timer->mTimerMask = 0;
#endif
  OgreTimerReset(timer);
}

//-------------------------------------------------------------------------
void OgreTimerUninit(struct OgreTimer *timer) {}

//-------------------------------------------------------------------------
#if 0
int OgreTimerSetOption(struct OgreTimer *timer, const char * key, const void * val )
{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	if (0 == strcmp(key, "QueryAffinityMask"))
	{
		// Telling timer what core to use for a timer read
		DWORD newTimerMask = * (const DWORD *) ( val );

		// Get the current process core mask
		DWORD_PTR procMask;
		DWORD_PTR sysMask;
		GetProcessAffinityMask(GetCurrentProcess(), &procMask, &sysMask);

		// If new mask is 0, then set to default behavior, otherwise check
		// to make sure new timer core mask overlaps with process core mask
		// and that new timer core mask is a power of 2 (i.e. a single core)
		if( ( newTimerMask == 0 ) ||
			( ( ( newTimerMask & procMask ) != 0 ) && Bitwise::isPO2( newTimerMask ) ) )
		{
			timer->mTimerMask = newTimerMask;
			return 1;
		}
	}
#endif

	return 0;
}
#endif

//-------------------------------------------------------------------------
void OgreTimerReset(struct OgreTimer *timer)
{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    // Get the current process core mask
	DWORD_PTR procMask;
	DWORD_PTR sysMask;
	GetProcessAffinityMask(GetCurrentProcess(), &procMask, &sysMask);

	// If procMask is 0, consider there is only one core available
	// (using 0 as procMask will cause an infinite loop below)
	if (procMask == 0)
		procMask = 1;

	// Find the lowest core that this process uses
	if( timer->mTimerMask == 0 )
	{
		timer->mTimerMask = 1;
		while( ( timer->mTimerMask & procMask ) == 0 )
		{
			timer->mTimerMask <<= 1;
		}
	}

    {
	HANDLE thread = GetCurrentThread();

	// Set affinity to the first core
	DWORD_PTR oldMask = SetThreadAffinityMask(thread, timer->mTimerMask);
#endif

	// Get the constant frequency
	QueryPerformanceFrequency(&timer->mFrequency);

	// Query the timer
	QueryPerformanceCounter(&timer->mStartTime);
	timer->mStartTick = GetTickCount();

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	// Reset affinity
	SetThreadAffinityMask(thread, oldMask);
#endif
    }

	timer->mLastTime = 0;
	timer->mZeroClock = clock();
}

//-------------------------------------------------------------------------
unsigned long OgreTimerGetMilliseconds(struct OgreTimer *timer)
{
    LARGE_INTEGER curTime;

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	HANDLE thread = GetCurrentThread();

	// Set affinity to the first core
	DWORD_PTR oldMask = SetThreadAffinityMask(thread, timer->mTimerMask);
#endif

	// Query the timer
	QueryPerformanceCounter(&curTime);

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	// Reset affinity
	SetThreadAffinityMask(thread, oldMask);
#endif

    {
    LONGLONG newTime = curTime.QuadPart - timer->mStartTime.QuadPart;

    // scale by 1000 for milliseconds
    unsigned long newTicks = (unsigned long) (1000 * newTime / timer->mFrequency.QuadPart);

    // detect and compensate for performance counter leaps
    // (surprisingly common, see Microsoft KB: Q274323)
    unsigned long check = GetTickCount() - timer->mStartTick;
    signed long msecOff = (signed long)(newTicks - check);
    if (msecOff < -100 || msecOff > 100)
    {
        // We must keep the timer running forward :)
        LONGLONG adjust = MIN(msecOff * timer->mFrequency.QuadPart / 1000, newTime - timer->mLastTime);
        timer->mStartTime.QuadPart += adjust;
        newTime -= adjust;

        // Re-calculate milliseconds
        newTicks = (unsigned long) (1000 * newTime / timer->mFrequency.QuadPart);
    }

    // Record last time for adjust
    timer->mLastTime = newTime;

    return newTicks;
    }
}

//-------------------------------------------------------------------------
unsigned long OgreTimerGetMicroseconds(struct OgreTimer *timer)
{
    LARGE_INTEGER curTime;

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	HANDLE thread = GetCurrentThread();

	// Set affinity to the first core
	DWORD_PTR oldMask = SetThreadAffinityMask(thread, timer->mTimerMask);
#endif

	// Query the timer
	QueryPerformanceCounter(&curTime);

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	// Reset affinity
	SetThreadAffinityMask(thread, oldMask);
#endif

    {
    unsigned long newMicro;
	LONGLONG newTime = curTime.QuadPart - timer->mStartTime.QuadPart;

    // get milliseconds to check against GetTickCount
    unsigned long newTicks = (unsigned long) (1000 * newTime / timer->mFrequency.QuadPart);

    // detect and compensate for performance counter leaps
    // (surprisingly common, see Microsoft KB: Q274323)
    unsigned long check = GetTickCount() - timer->mStartTick;
    signed long msecOff = (signed long)(newTicks - check);
    if (msecOff < -100 || msecOff > 100)
    {
        // We must keep the timer running forward :)
        LONGLONG adjust = MIN(msecOff * timer->mFrequency.QuadPart / 1000, newTime - timer->mLastTime);
        timer->mStartTime.QuadPart += adjust;
        newTime -= adjust;
    }

    // Record last time for adjust
    timer->mLastTime = newTime;

    // scale by 1000000 for microseconds
    newMicro = (unsigned long) (1000000 * newTime / timer->mFrequency.QuadPart);

    return newMicro;
    }
}

//-------------------------------------------------------------------------
unsigned long OgreTimerGetMillisecondsCPU(struct OgreTimer *timer)
{
	clock_t newClock = clock();
	return (unsigned long)( (float)( newClock - timer->mZeroClock ) / ( (float)CLOCKS_PER_SEC / 1000.0 ) ) ;
}

//-------------------------------------------------------------------------
unsigned long OgreTimerGetMicrosecondsCPU(struct OgreTimer *timer)
{
	clock_t newClock = clock();
	return (unsigned long)( (float)( newClock - timer->mZeroClock ) / ( (float)CLOCKS_PER_SEC / 1000000.0 ) ) ;
}
