/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <array>
#include <mutex>
#include <wtf/IteratorRange.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OSRandomSource.h>

namespace WTF {

namespace {

class ARC4Stream {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ARC4Stream();

    uint8_t i { 0 };
    uint8_t j { 0 };
    std::array<uint8_t, 256> s;
};

class ARC4RandomNumberGenerator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ARC4RandomNumberGenerator() = default;

    template<typename IntegerType>
    IntegerType randomNumber();
    void randomValues(std::span<uint8_t>);

private:
    inline void addRandomData(std::span<const uint8_t, 128>);
    void stir() WTF_REQUIRES_LOCK(m_lock);
    void stirIfNeeded() WTF_REQUIRES_LOCK(m_lock);
    inline uint8_t getByte() WTF_REQUIRES_LOCK(m_lock);

    Lock m_lock;
    ARC4Stream m_stream;
    int m_count { 0 };
};

ARC4Stream::ARC4Stream()
{
    for (unsigned n = 0; n < 256; ++n)
        s[n] = n;
}

void ARC4RandomNumberGenerator::addRandomData(std::span<const uint8_t, 128> data)
{
    --m_stream.i;
    for (unsigned n = 0; n < 256; ++n) {
        ++m_stream.i;
        uint8_t si = m_stream.s[m_stream.i];
        m_stream.j += si + data[n % data.size()];
        m_stream.s[m_stream.i] = m_stream.s[m_stream.j];
        m_stream.s[m_stream.j] = si;
    }
    m_stream.j = m_stream.i;
}

void ARC4RandomNumberGenerator::stir()
{
    std::array<uint8_t, 128> randomness;
    cryptographicallyRandomValuesFromOS(randomness);
    addRandomData(randomness);

    // Discard early keystream, as per recommendations in:
    // http://www.wisdom.weizmann.ac.il/~itsik/RC4/Papers/Rc4_ksa.ps
    for (unsigned i = 0; i < 256; ++i)
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

template<typename IntegerType>
IntegerType ARC4RandomNumberGenerator::randomNumber()
{
    Locker locker { m_lock };

    IntegerType val = 0;
    for (unsigned i = 0; i < sizeof(IntegerType); ++i) {
        m_count--;
        stirIfNeeded();
        val = (val << 8) | getByte();
    }

    return val;
}

void ARC4RandomNumberGenerator::randomValues(std::span<uint8_t> buffer)
{
    Locker locker { m_lock };

    for (auto& byte : WTF::makeReversedRange(buffer)) {
        m_count--;
        stirIfNeeded();
        byte = getByte();
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

template<> uint8_t cryptographicallyRandomNumber<uint8_t>()
{
    return sharedRandomNumberGenerator().randomNumber<uint8_t>();
}

template<> unsigned cryptographicallyRandomNumber<unsigned>()
{
    return sharedRandomNumberGenerator().randomNumber<unsigned>();
}

void cryptographicallyRandomValues(std::span<uint8_t> buffer)
{
    sharedRandomNumberGenerator().randomValues(buffer);
}

template<> uint64_t cryptographicallyRandomNumber<uint64_t>()
{
    return sharedRandomNumberGenerator().randomNumber<uint64_t>();
}

double cryptographicallyRandomUnitInterval()
{
    return cryptographicallyRandomNumber<unsigned>() / (std::numeric_limits<unsigned>::max() + 1.0);
}

}
