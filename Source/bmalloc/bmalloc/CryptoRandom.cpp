/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Arc4 random number generator for OpenBSD.
 *
 * This code is derived from section 17.1 of Applied Cryptography,
 * second edition, which describes a stream cipher allegedly
 * compatible with RSA Labs "RC4" cipher (the actual description of
 * which is a trade secret).  The same algorithm is used as a stream
 * cipher called "arcfour" in Tatu Ylonen's ssh package.
 *
 * RC4 is a registered trademark of RSA Laboratories.
 */

#include "CryptoRandom.h"

#include "BAssert.h"
#include "BPlatform.h"
#include "Mutex.h"
#include "StaticPerProcess.h"
#include "VMAllocate.h"
#include <mutex>

#if !BOS(DARWIN)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if BOS(DARWIN)
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonRandom.h>
#endif

namespace bmalloc {

class ARC4Stream {
public:
    ARC4Stream();

    uint8_t i;
    uint8_t j;
    uint8_t s[256];
};

class ARC4RandomNumberGenerator : public StaticPerProcess<ARC4RandomNumberGenerator> {
public:
    ARC4RandomNumberGenerator(const LockHolder&);

    uint32_t randomNumber();
    void randomValues(void* buffer, size_t length);

private:
    inline void addRandomData(unsigned char *data, int length);
    void stir();
    void stirIfNeeded();
    inline uint8_t getByte();

    ARC4Stream m_stream;
    int m_count;
};
DECLARE_STATIC_PER_PROCESS_STORAGE(ARC4RandomNumberGenerator);
DEFINE_STATIC_PER_PROCESS_STORAGE(ARC4RandomNumberGenerator);

ARC4Stream::ARC4Stream()
{
    for (int n = 0; n < 256; n++)
        s[n] = n;
    i = 0;
    j = 0;
}

ARC4RandomNumberGenerator::ARC4RandomNumberGenerator(const LockHolder&)
    : m_count(0)
{
}

void ARC4RandomNumberGenerator::addRandomData(unsigned char* data, int length)
{
    m_stream.i--;
    for (int n = 0; n < 256; n++) {
        m_stream.i++;
        uint8_t si = m_stream.s[m_stream.i];
        m_stream.j += si + data[n % length];
        m_stream.s[m_stream.i] = m_stream.s[m_stream.j];
        m_stream.s[m_stream.j] = si;
    }
    m_stream.j = m_stream.i;
}

void ARC4RandomNumberGenerator::stir()
{
    unsigned char randomness[128];
    size_t length = sizeof(randomness);

#if BOS(DARWIN)
    RELEASE_BASSERT(!CCRandomGenerateBytes(randomness, length));
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
        ssize_t currentRead = read(fd, randomness + amountRead, length - amountRead);
        // We need to check for both EAGAIN and EINTR since on some systems /dev/urandom
        // is blocking and on others it is non-blocking.
        if (currentRead == -1)
            RELEASE_BASSERT(errno == EAGAIN || errno == EINTR);
        else
            amountRead += currentRead;
    }
#endif

    addRandomData(randomness, length);

    // Discard early keystream, as per recommendations in:
    // http://www.wisdom.weizmann.ac.il/~itsik/RC4/Papers/Rc4_ksa.ps
    for (int i = 0; i < 256; i++)
        getByte();
    m_count = 1600000;
}

void ARC4RandomNumberGenerator::stirIfNeeded()
{
    if (m_count <= 0)
        stir();
}

uint8_t ARC4RandomNumberGenerator::getByte()
{
    m_stream.i++;
    uint8_t si = m_stream.s[m_stream.i];
    m_stream.j += si;
    uint8_t sj = m_stream.s[m_stream.j];
    m_stream.s[m_stream.i] = sj;
    m_stream.s[m_stream.j] = si;
    return (m_stream.s[(si + sj) & 0xff]);
}

void ARC4RandomNumberGenerator::randomValues(void* buffer, size_t length)
{
    LockHolder lock(mutex());

    unsigned char* result = reinterpret_cast<unsigned char*>(buffer);
    stirIfNeeded();
    while (length--) {
        m_count--;
        stirIfNeeded();
        result[length] = getByte();
    }
}

void cryptoRandom(void* buffer, size_t length)
{
    ARC4RandomNumberGenerator::get()->randomValues(buffer, length);
}

} // namespace bmalloc

