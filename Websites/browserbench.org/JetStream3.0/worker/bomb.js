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
    if (!JetStream.isInBrowser)
        throw new Error("Only works in browser");

    const tests = [
        JetStream.preload.rayTrace3D,
        JetStream.preload.accessNbody,
        JetStream.preload.morph3D,
        JetStream.preload.cube3D,
        JetStream.preload.accessFunnkuch,
        JetStream.preload.accessBinaryTrees,
        JetStream.preload.accessNsieve,
        JetStream.preload.bitopsBitwiseAnd,
        JetStream.preload.bitopsNsieveBits,
        JetStream.preload.controlflowRecursive,
        JetStream.preload.bitops3BitBitsInByte,
        JetStream.preload.botopsBitsInByte,
        JetStream.preload.cryptoAES,
        JetStream.preload.cryptoMD5,
        JetStream.preload.cryptoSHA1,
        JetStream.preload.dateFormatTofte,
        JetStream.preload.dateFormatXparb,
        JetStream.preload.mathCordic,
        JetStream.preload.mathPartialSums,
        JetStream.preload.mathSpectralNorm,
        JetStream.preload.stringBase64,
        JetStream.preload.stringFasta,
        JetStream.preload.stringValidateInput,
        JetStream.preload.stringTagcloud,
        JetStream.preload.stringUnpackCode,
        JetStream.preload.regexpDNA,
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
