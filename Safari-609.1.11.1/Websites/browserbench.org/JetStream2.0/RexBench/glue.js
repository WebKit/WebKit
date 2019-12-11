/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
"use strict";

const driver = new Driver(
    isInBrowser ? document.getElementById("status") : null,
    isInBrowser ? document.getElementById("trigger") : null,
    function() {
        driver.start(6)
    },
    isInBrowser ? document.getElementById("magic") : null,
    isInBrowser ? document.getElementById("Geomean") : null,
    "sampleBench");

function reportResult(...args) {
    driver.reportResult(...args);
}

{
    const title = "RexBench 0.93";
    if (isInBrowser) {
        document.title = title;
    } else {
        print(title);
    }
}

driver.addBenchmark(RegexDNABenchmarkRunner);
driver.addBenchmark(Octane2RegExpBenchmarkRunner);
driver.addBenchmark(OfflineAssemblerBenchmarkRunner);
driver.addBenchmark(UniPokerBenchmarkRunner);
driver.addBenchmark(FlightPlannerBenchmarkRunner);
driver.addBenchmark(FlightPlannerUnicodeBenchmarkRunner);
driver.readyTrigger();
