#pragma once
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace util {
    class FastMutex {
        CRITICAL_SECTION crit;
    public:
        FastMutex();
        FastMutex(const FastMutex&) = delete;
        FastMutex(const FastMutex&&) = delete;
        ~FastMutex();

        void lock();
        void unlock();
    };
}