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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <chrono>
#include <random>
#include <set>
#include <wtf/MediaTime.h>

using namespace std;
using namespace std::chrono;

static const size_t setSize = 100000;

void performTest(const char* name, function<void()> test)
{
    vector<double> runtimes(21);
    for (auto& runtime : runtimes) {
        auto start = steady_clock::now();
        test();
        runtime = duration_cast<milliseconds>(steady_clock::now() - start).count();
    }
    sort(runtimes.begin(), runtimes.end());
    double sum = std::accumulate(runtimes.begin(), runtimes.end(), 0);
    double mean = sum / runtimes.size();
    double median = runtimes[(runtimes.size() + 1) / 2];
    double min = runtimes.front();
    double max = runtimes.back();
    double sqSum = std::inner_product(runtimes.begin(), runtimes.end(), runtimes.begin(), 0);
    double stdev = std::sqrt(sqSum / runtimes.size() - mean * mean);

    printf("RESULT %s: Time= %g ms", name, sum);
    printf("median= %g ms, stdev= %g ms, min= %g ms, max = %g ms", median, stdev, min, max);
}

void test(int32_t count, function<MediaTime(int32_t)> generator)
{
    set<MediaTime> times;

    for (int32_t i = 0; i < count; ++i)
        times.insert(generator(i));

    for (int32_t i = 0; i < count; ++i)
        times.upper_bound(generator(i));
}

int main(int argc, const char * argv[])
{
    performTest("Equal TimeScales", [] { test(setSize, [] (int32_t i) { return MediaTime(i, 1); }); });
    performTest("Equal TimeValues", [] { test(setSize, [] (int32_t i) { return MediaTime(1, i + 1); }); });
    performTest("Disparate TimeValues & TimeScales", [] { test(setSize, [] (int32_t i) { return MediaTime(i, i + 1); }); });
    performTest("Non-uniform", [] {
        test(setSize, [] (int32_t i) {
            switch (i % 6) {
            case 0:
                return MediaTime::invalidTime();
            case 1:
                return MediaTime::positiveInfiniteTime();
            case 2:
                return MediaTime::negativeInfiniteTime();
            case 3:
                return MediaTime::indefiniteTime();
            case 4:
                return MediaTime(i, 1);
            case 5:
            default:
                return MediaTime(i, i + 1);
            }
        });
    });

    return 0;
}
