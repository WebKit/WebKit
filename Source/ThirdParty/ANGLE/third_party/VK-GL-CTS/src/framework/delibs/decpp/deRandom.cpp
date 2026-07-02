/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Random number generator utilities.
 *//*--------------------------------------------------------------------*/

#include "deRandom.hpp"
#include "deMemory.h"

#include <cstdint>

inline bool operator==(const deRandom &a, const deRandom &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

inline bool operator!=(const deRandom &a, const deRandom &b)
{
    return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}

namespace de
{

bool Random::operator==(const Random &other) const
{
    return m_rnd == other.m_rnd;
}

bool Random::operator!=(const Random &other) const
{
    return m_rnd != other.m_rnd;
}

void Random_selfTest(void)
{
    // getBool()

    {
        static const bool expected[] = {true,  false, false, false, true, true,  false, false, false, false,
                                        false, false, true,  false, true, false, false, false, false, true};
        Random rnd(4789);
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(expected[i] == rnd.getBool());
    }

    // getInt(a, b)

    {
        static const int expected[] = {-6628, -6483, 802, -7758, -8463, 3165, 9216, 3107, 1851, 8707};
        Random rnd(4789);
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(expected[i] == rnd.getInt(-10000, 10000));
    }

    // getUint32()

    {
        static const uint32_t expected[] = {3694588092u, 3135240271u, 882874943u,  2108407657u, 376640368u,
                                            1395362929u, 2611849801u, 3151830690u, 901476922u,  989608184u};
        Random rnd(4789);
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(expected[i] == rnd.getUint32());
    }

    // getUint64()

    {
        static const uint64_t expected[] = {15868135030466279503ull, 3791919008751271785ull,  1617658064308767857ull,
                                            11217809480510938786ull, 3871813899078351096ull,  14768747990643252542ull,
                                            8163484985646009214ull,  14928018127607458387ull, 432108271545246292ull,
                                            7318152987070448462ull};
        Random rnd(4789);
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(expected[i] == rnd.getUint64());
    }

    // getFloat()

    {
        static const float expected[] = {0.763413f, 0.679680f, 0.288965f, 0.854431f, 0.403095f,
                                         0.198132f, 0.729899f, 0.741484f, 0.358263f, 0.686578f};
        const float epsilon           = 0.01f;
        Random rnd(4789);
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(de::abs(expected[i] - rnd.getFloat()) < epsilon);
    }

    // getFloat(a, b)

    {
        static const float expected[] = {824.996643f,  675.039185f, -24.691774f, 987.999756f, 179.702286f,
                                         -187.365463f, 764.975647f, 785.724182f, 99.413582f,  687.392151f};
        const float epsilon           = 0.01f;
        Random rnd(4789);
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(de::abs(expected[i] - rnd.getFloat(-542.2f, 1248.7f)) < epsilon);
    }

    // choose(first, last, resultOut, num)

    {
        static const int items[]                    = {3, 42, 45, 123, 654, -123, -90, 0, 43};
        const int numItemsPicked                    = 5;
        static const int expected[][numItemsPicked] = {
            {-123, 42, -90, 123, 43}, {43, 0, -90, 123, -123}, {3, 42, 45, 123, 0},    {3, 42, 45, -123, 654},
            {3, 43, 45, -90, -123},   {-90, 0, 45, -123, 654}, {3, 42, 43, 123, -123}, {-90, 43, 45, 123, 654},
            {0, 42, 45, 123, 654},    {0, -90, 45, -123, 654}};
        Random rnd(4789);

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
        {
            int itemsDst[numItemsPicked];
            rnd.choose(DE_ARRAY_BEGIN(items), DE_ARRAY_END(items), &itemsDst[0], numItemsPicked);
            for (int j = 0; j < numItemsPicked; j++)
                DE_TEST_ASSERT(expected[i][j] == itemsDst[j]);
        }
    }

    // choose(first, last)

    {
        static const int items[]    = {3, 42, 45, 123, 654, -123, -90, 0, 43};
        static const int expected[] = {43, 123, -90, -90, 0, 3, 43, 0, 654, 43};
        Random rnd(4789);

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(expected[i] == rnd.choose<int>(DE_ARRAY_BEGIN(items), DE_ARRAY_END(items)));
    }

    // chooseWeighted(first, last, weights)

    {
        static const int items[]     = {3, 42, 45, 123, 654, -123, -90, 0};
        static const float weights[] = {0.4f, 0.6f, 1.5f, 0.5f, 1.2f, 0.3f, 0.2f, 1.4f};
        DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(items) == DE_LENGTH_OF_ARRAY(weights));
        static const int expected[] = {-90, 654, 45, 0, 45, 45, -123, -90, 45, 654};
        Random rnd(4789);

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
            DE_TEST_ASSERT(expected[i] ==
                           rnd.chooseWeighted<int>(DE_ARRAY_BEGIN(items), DE_ARRAY_END(items), &weights[0]));
    }

    // suffle()

    {
        int items[]                                            = {3, 42, 45, 123, 654, -123, -90, 0, 43};
        static const int expected[][DE_LENGTH_OF_ARRAY(items)] = {
            {45, 43, 654, -123, 123, 42, -90, 0, 3}, {0, 43, 3, 42, -123, -90, 654, 45, 123},
            {42, 43, 654, 3, 0, 123, -90, -123, 45}, {3, 45, 43, 42, 123, 654, 0, -90, -123},
            {42, 45, -123, 0, -90, 654, 3, 123, 43}, {654, -123, 3, 42, 43, 0, -90, 123, 45},
            {0, 3, 654, 42, -90, 45, -123, 123, 43}, {654, 3, 45, 42, -123, -90, 123, 43, 0},
            {-90, 123, 43, 654, 0, 42, 45, 3, -123}, {0, -123, 45, 42, 43, 123, 3, -90, 654}};
        Random rnd(4789);

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(expected); i++)
        {
            rnd.shuffle(DE_ARRAY_BEGIN(items), DE_ARRAY_END(items));
            for (int j = 0; j < DE_LENGTH_OF_ARRAY(items); j++)
                DE_TEST_ASSERT(expected[i][j] == items[j]);
        }
    }
}

void fillWithRandomData(de::Random &rnd, void *data, size_t size)
{
    char *bytes      = reinterpret_cast<char *>(data);
    size_t remaining = size;

    // Proceed in blocks of 4 bytes, then 2 bytes, then single bytes.

    while (remaining >= sizeof(uint32_t))
    {
        const auto value = rnd.getUint32();
        deMemcpy(bytes, &value, sizeof(value));
        remaining -= sizeof(value);
        bytes += sizeof(value);
    }

    while (remaining >= sizeof(uint16_t))
    {
        const auto value = rnd.getUint16();
        deMemcpy(bytes, &value, sizeof(value));
        remaining -= sizeof(value);
        bytes += sizeof(value);
    }

    while (remaining >= sizeof(uint8_t))
    {
        const auto value = rnd.getUint8();
        deMemcpy(bytes, &value, sizeof(value));
        remaining -= sizeof(value);
        bytes += sizeof(value);
    }
}

} // namespace de
