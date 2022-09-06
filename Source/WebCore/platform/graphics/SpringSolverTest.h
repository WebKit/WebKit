/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "SpringSolver.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

void assertEquals(double expected, double actual)
{
    if (fabs(expected - actual) > 0.001) {
        printf("%g != %g\n", expected, actual);
        assert(false);
    }
}

struct Coordinate {
    double time;
    double value;
};

double settlingTime(std::vector<Coordinate> series)
{
    for (auto itr = series.rbegin(); itr != series.rend(); ++itr) {
        if (itr->value > 1.02 || itr->value < 0.98)
            return itr->time;
    }
    return 0;
}

Coordinate max(std::vector<Coordinate> series)
{
    Coordinate max { 0, 0 };
    for (auto itr = series.begin(); itr != series.end(); ++itr) {
        if (itr->value > max.value) {
            max.value = itr->value;
            max.time = itr->time;
        }
    }
    return max;
}

void testUnderDamped()
{
    WebCore::SpringSolver subject(1, 150, 12.2, 0);
    std::vector<Coordinate> series;
    for (double t = 0; t < 1; t += 0.001) {
        Coordinate c;
        c.time = t;
        c.value = subject.solve(t);
        series.push_back(c);
    }

    assertEquals(0.66, settlingTime(series));
    assertEquals(0.296, max(series).time);
    assertEquals(1.164, max(series).value);
}

void testCriticallyDamped()
{
    WebCore::SpringSolver subject(1, 150, 2 * std::sqrt(150), 0);
    std::vector<Coordinate> series;
    for (double t = 0; t < 1; t += 0.001) {
        Coordinate c;
        c.time = t;
        c.value = subject.solve(t);
        series.push_back(c);
    }

    assertEquals(0.476, settlingTime(series));
    assertEquals(0.999, max(series).time);
    assertEquals(0.999, max(series).value);
}

void testOverDamped()
{
    WebCore::SpringSolver subject(1, 150, 36, 0);
    std::vector<Coordinate> series;
    for (double t = 0; t < 1; t += 0.001) {
        Coordinate c;
        c.time = t;
        c.value = subject.solve(t);
        series.push_back(c);
    }

    assertEquals(0.848, settlingTime(series));
    assertEquals(0.999, max(series).time);
    assertEquals(0.990, max(series).value);
}

int main()
{
    testUnderDamped();
    testCriticallyDamped();
    testOverDamped();
    return 0;
}
