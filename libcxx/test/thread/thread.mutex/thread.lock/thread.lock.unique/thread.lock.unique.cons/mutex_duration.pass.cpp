//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <mutex>

// class timed_mutex;

// template <class Rep, class Period>
//   unique_lock(mutex_type& m, const chrono::duration<Rep, Period>& rel_time);

#include <mutex>
#include <thread>
#include <cstdlib>
#include <cassert>

std::timed_mutex m;

typedef std::chrono::monotonic_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef std::chrono::milliseconds ms;
typedef std::chrono::nanoseconds ns;

void f1()
{
    time_point t0 = Clock::now();
    std::unique_lock<std::timed_mutex> lk(m, ms(300));
    assert(lk.owns_lock() == true);
    time_point t1 = Clock::now();
    ns d = t1 - t0 - ms(250);
    assert(d < ns(5000000));  // within 5ms
}

void f2()
{
    time_point t0 = Clock::now();
    std::unique_lock<std::timed_mutex> lk(m, ms(250));
    assert(lk.owns_lock() == false);
    time_point t1 = Clock::now();
    ns d = t1 - t0 - ms(250);
    assert(d < ns(5000000));  // within 5ms
}

int main()
{
    {
        m.lock();
        std::thread t(f1);
        std::this_thread::sleep_for(ms(250));
        m.unlock();
        t.join();
    }
    {
        m.lock();
        std::thread t(f2);
        std::this_thread::sleep_for(ms(300));
        m.unlock();
        t.join();
    }
}
