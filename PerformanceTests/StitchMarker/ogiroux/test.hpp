/*

Copyright (c) 2017, NVIDIA Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TEST_HPP
#define TEST_HPP

template<class Sem>
struct semaphore_mutex {
    __test_abi void lock() {
        __sem.acquire();
    }
    __test_abi void unlock() {
        __sem.release();
    }
    Sem __sem{ 1 };
};

using binary_semaphore_mutex = semaphore_mutex<binary_semaphore>;
using counting_semaphore_mutex = semaphore_mutex<counting_semaphore>;

#ifndef __builtin_expect
#define __builtin_expect(x,y) (x)
#endif

struct synchronic_mutex {

    __test_abi void lock() {

        bool state;
        while(1) {
            if (__builtin_expect(__locked.compare_exchange_weak(state = false, true, std::memory_order_acquire), 1))
                break;
            if(state)
                atomic_wait_explicit(&__locked, state, std::memory_order_relaxed);
        }
    }

    __test_abi void unlock() {

        __locked.store(false, std::memory_order_release);
        atomic_signal(&__locked);
    }

    atomic<bool> __locked{ false };
};

struct poor_mutex {

    __test_abi void lock() {

        bool state;
        while (!__locked.compare_exchange_weak(state = false, true, std::memory_order_acquire))
            ;
    }

    __test_abi void unlock() {

        __locked.store(false, std::memory_order_release);
    }

    atomic<bool> __locked{ false };
};

#endif //TEST_HPP
