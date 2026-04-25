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

class Benchmark {
    constructor() {
        this._verbose = false;
        this._c1DecryptedIsCorrect = true;
        this._c2DecryptedIsCorrect = true;
        this._decryptedMulIsCorrect = true;
    }

    runIteration() {
        const { publicKey, privateKey } = paillierBigint.generateRandomKeysSync(1024);

        const m1 = 12345678901234567890n;
        const m2 = 5n;

        const c1 = publicKey.encrypt(m1);
        const c1Decrypted = privateKey.decrypt(c1);
        this._c1DecryptedIsCorrect &&= c1Decrypted === 12345678901234567890n;

        const c2 = publicKey.encrypt(m2);
        const encryptedSum = publicKey.addition(c1, c2);
        const c2Decrypted = privateKey.decrypt(encryptedSum);
        this._c2DecryptedIsCorrect &&= c2Decrypted === 12345678901234567895n;

        const k = 10n;
        const encryptedMul = publicKey.multiply(c1, k);
        const decryptedMul = privateKey.decrypt(encryptedMul);
        this._decryptedMulIsCorrect &&= decryptedMul === 123456789012345678900n;
    }

    validate() {
        if (!this._c1DecryptedIsCorrect)
            throw new Error("Bad value: c1Decrypted!");

        if (!this._c2DecryptedIsCorrect)
            throw new Error("Bad value: c2Decrypted!");

        if (!this._decryptedMulIsCorrect)
            throw new Error("Bad value: decryptedMul!");
    }
}
