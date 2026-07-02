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
 * \brief Ring buffer template.
 *//*--------------------------------------------------------------------*/

#include "deRingBuffer.hpp"
#include "deRandom.hpp"

#include <vector>

using std::vector;

namespace de
{

void RingBuffer_selfTest(void)
{
    const int numIterations = 16;

    for (int iterNdx = 0; iterNdx < numIterations; iterNdx++)
    {
        Random rnd(iterNdx);
        int bufSize  = rnd.getInt(1, 2048);
        int dataSize = rnd.getInt(100, 10000);
        RingBuffer<int> buffer(bufSize);
        vector<int> data(dataSize);

        for (int i = 0; i < dataSize; i++)
            data[i] = i;

        int writePos = 0;
        int readPos  = 0;

        while (writePos < dataSize || readPos < dataSize)
        {
            bool canRead  = buffer.getNumElements() > 0;
            bool canWrite = writePos < dataSize && buffer.getNumFree() > 0;
            bool doRead   = canRead && (!canWrite || rnd.getBool());

            // Free count must match.
            DE_TEST_ASSERT(buffer.getNumFree() == bufSize - (writePos - readPos));

            if (doRead)
            {
                int numBytes = rnd.getInt(1, buffer.getNumElements());
                vector<int> tmp(numBytes);

                buffer.popBack(&tmp[0], numBytes);

                for (int i = 0; i < numBytes; i++)
                    DE_TEST_ASSERT(tmp[i] == data[readPos + i]);

                readPos += numBytes;
            }
            else
            {
                DE_TEST_ASSERT(canWrite);

                int numBytes = rnd.getInt(1, de::min(dataSize - writePos, buffer.getNumFree()));
                buffer.pushFront(&data[writePos], numBytes);
                writePos += numBytes;
            }
        }
    }
}

} // namespace de
