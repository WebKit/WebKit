/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "CryptoRandom.h"

#include "BAssert.h"
#include "BPlatform.h"
#include <mutex>

#if !BOS(DARWIN)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if BOS(DARWIN)
typedef struct __CCRandom* CCRandomRef;

extern "C" {
extern const CCRandomRef kCCRandomDefault;
int CCRandomCopyBytes(CCRandomRef rnd, void *bytes, size_t count);
}
#endif

namespace bmalloc {

void cryptoRandom(unsigned char* buffer, size_t length)
{
#if BOS(DARWIN)
    RELEASE_BASSERT(!CCRandomCopyBytes(kCCRandomDefault, buffer, length));
#else
    static std::once_flag onceFlag;
    static int fd;
    std::call_once(
        onceFlag,
        [] {
            int ret = 0;
            do {
                ret = open("/dev/urandom", O_RDONLY, 0);
            } while (ret == -1 && errno == EINTR);
            RELEASE_BASSERT(ret >= 0);
            fd = ret;
        });
    ssize_t amountRead = 0;
    while (static_cast<size_t>(amountRead) < length) {
        ssize_t currentRead = read(fd, buffer + amountRead, length - amountRead);
        // We need to check for both EAGAIN and EINTR since on some systems /dev/urandom
        // is blocking and on others it is non-blocking.
        if (currentRead == -1)
            RELEASE_BASSERT(errno == EAGAIN || errno == EINTR);
        else
            amountRead += currentRead;
    }
#endif
}

} // namespace bmalloc

