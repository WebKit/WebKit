/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
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

#include "config.h"
#include <wtf/CryptographicallyRandomNumber.h>

#include <mutex>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OSRandomSource.h>

namespace WTF {

namespace {

class ARC4Stream {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ARC4Stream();

    uint8_t i;
    uint8_t j;
    uint8_t s[256];
};

class ARC4RandomNumberGenerator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ARC4RandomNumberGenerator();

    uint32_t randomNumber();
    void randomValues(void* buffer, size_t length);

private:
    inline void addRandomData(unsigned char *data, int length);
    void stir() WTF_REQUIRES_LOCK(m_lock);
    void stirIfNeeded() WTF_REQUIRES_LOCK(m_lock);
    inline uint8_t getByte();
    inline uint32_t getWord();

    Lock m_lock;
    ARC4Stream m_stream;
    int m_count;
};

ARC4Stream::ARC4Stream()
{
    for (int n = 0; n < 256; n++)
        s[n] = n;
    i = 0;
    j = 0;
}

ARC4RandomNumberGenerator::ARC4RandomNumberGenerator()
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
    cryptographicallyRandomValuesFromOS(randomness, length);
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

uint32_t ARC4RandomNumberGenerator::getWord()
{
    uint32_t val;
    val = getByte() << 24;
    val |= getByte() << 16;
    val |= getByte() << 8;
    val |= getByte();
    return val;
}

uint32_t ARC4RandomNumberGenerator::randomNumber()
{
    Locker locker { m_lock };

    m_count -= 4;
    stirIfNeeded();
    return getWord();
}

void ARC4RandomNumberGenerator::randomValues(void* buffer, size_t length)
{
    Locker locker { m_lock };

    auto result = static_cast<unsigned char*>(buffer);
    stirIfNeeded();
    while (length--) {
        m_count--;
        stirIfNeeded();
        result[length] = getByte();
    }
}

ARC4RandomNumberGenerator& sharedRandomNumberGenerator()
{
    static LazyNeverDestroyed<ARC4RandomNumberGenerator> randomNumberGenerator;
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            randomNumberGenerator.construct();
        });

    return randomNumberGenerator;
}

}

uint32_t cryptographicallyRandomNumber()
{
    return sharedRandomNumberGenerator().randomNumber();
}

void cryptographicallyRandomValues(void* buffer, size_t length)
{
    sharedRandomNumberGenerator().randomValues(buffer, length);
}

}
