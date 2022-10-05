/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

class Benchmark {
    constructor()
    {
        this._verbose = false;
        
        this._payloads = [
            {generate: createPayloadGbemuExecuteIteration, earlyHash: 632653144, lateHash: 372715518},
            {generate: createPayloadImagingGaussianBlurGaussianBlur, earlyHash: 3677819581, lateHash: 1252116304},
            {generate: createPayloadTypescriptScanIdentifier, earlyHash: 1914852601, lateHash: 837339551},
            {generate: createPayloadAirJSACLj8C, earlyHash: 1373599940, lateHash: 3981283600}
        ];
    }
    
    runIteration()
    {
        for (let payload of this._payloads) {
            // Sadly about 17% of our time is in generate. I don't think that's really avoidable,
            // and I don't mind testing VMs' ability to run such "data definition" code quickly. I
            // would not have expected it to be so slow from first principles!
            let code = payload.generate();
            
            if (this._verbose) {
                print("Before allocateStack:");
                print(code);
            }
            
            let hash = code.hash();
            if (hash != payload.earlyHash)
                throw new Error(`Wrong early hash for ${payload.generate.name}: ${hash}`);
            
            allocateStack(code);
            
            if (this._verbose) {
                print("After allocateStack:");
                print(code);
            }

            hash = code.hash();
            if (hash != payload.lateHash)
                throw new Error(`Wrong late hash for ${payload.generate.name}: ${hash}`);
        }
    }
}
