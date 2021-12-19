#include "mutex.h"

namespace util {
	FastMutex::FastMutex()
	{
		InitializeCriticalSection(&crit);
	}
	FastMutex::~FastMutex()
	{
		DeleteCriticalSection(&crit);
	}
	void FastMutex::lock()
	{
		EnterCriticalSection(&crit);
	}
	void FastMutex::unlock()
	{
		LeaveCriticalSection(&crit);
	}
}