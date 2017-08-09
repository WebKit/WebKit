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

class BasicBenchmark extends Benchmark {
    constructor(verbose = 0)
    {
        super(verbose);
    }
    
    runOnce()
    {
        prepare("10 print \"hello, world!\"\n20 end");
        prepare("10 let x = 0\n20 let x = x + 1\n30 print x\n40 if x < 10 then 20\n50 end");
        prepare("10 print int(rnd * 100)\n20 end\n");
        prepare("10 let value = int(rnd * 2000)\n20 print value\n30 if value <> 100 then 10\n40 end");
    for (let i = 0; i < 100; ++i)
        prepare("10 dim a(2000)\n20 for i = 2 to 2000\n30 let a(i) = 1\n40 next i\n50 for i = 2 to sqr(2000)\n60 if a(i) = 0 then 100\n70 for j = i ^ 2 to 2000 step i\n80 let a(j) = 0\n90 next j\n100 next i\n110 for i = 2 to 2000\n120 if a(i) = 0 then 140\n130 print i\n140 next i\n150 end\n");
    }
}
