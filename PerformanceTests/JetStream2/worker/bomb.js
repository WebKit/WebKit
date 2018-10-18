/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

let cycleCount = 20;

let resolve = null;

let numWorkers = 0;
function startWorker(file) {
    numWorkers++;
    let worker = new Worker(file);
    worker.onmessage = function(event) {
        if (event.data === "done") {
            --numWorkers;
            if (!numWorkers)
                resolve();
        }
    };
}

function startCycle() {
    if (!isInBrowser)
        throw new Error("Only works in browser");

    const tests = [
        rayTrace3D
        , accessNbody
        , morph3D
        , cube3D
        , accessFunnkuch
        , accessBinaryTrees
        , accessNsieve
        , bitopsBitwiseAnd
        , bitopsNsieveBits
        , controlflowRecursive
        , bitops3BitBitsInByte
        , botopsBitsInByte
        , cryptoAES
        , cryptoMD5
        , cryptoSHA1
        , dateFormatTofte
        , dateFormatXparb
        , mathCordic
        , mathPartialSums
        , mathSpectralNorm
        , stringBase64
        , stringFasta
        , stringValidateInput
        , stringTagcloud
        , stringUnpackCode
        , regexpDNA
    ];

    for (let test of tests)
        startWorker(test);

}


class Benchmark {
    async runIteration() {
        if (numWorkers !== 0 || resolve)
            throw new Error("Something bad happened.");

        let promise = new Promise((res) => resolve = res);
        startCycle();
        await promise;
        resolve = null;
    }
}
