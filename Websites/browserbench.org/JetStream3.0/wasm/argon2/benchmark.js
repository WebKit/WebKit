/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

let passwordStrings = [
    '123456',
    'p@assw0rd',
    'qwerty',
    'letmein',
    '汉字漢字',
    'كلمة المرور',
    'Z7ihQxGE93',
    '0pjTjkrnsM',
    '6Kg3AmWnVc',
];

// Emscripten doesn't have a way to manage your pointers for you so these will free them when the wrapper dies.

// If/when https://github.com/tc39/proposal-explicit-resource-management becomes a thing we should use that syntax/symbol instead of a dispose call.
class CString {
    constructor(string) {
        this.ptr = Module.stringToNewUTF8(string);
        this.length = Module._strlen(this.ptr);
    }

    dispose() {
        Module._free(this.ptr);
    }
}

class MallocPtr {
    constructor(size) {
        this.ptr = Module._malloc(size);
        this.size = size;
    }

    dispose() {
        Module._free(this.ptr);
    }
}

const tCost = 3;
const mCost = 1024;
const parallelism = 1;
// There are three argon2 types (modes), but they all exercise the same computational kernel,
// so we chose the recommended one from a security standpoint.
// See wasm/argon2/include/argon2.h for the enum:
// typedef enum Argon2_type {
//   Argon2_d = 0,
//   Argon2_i = 1,
//   Argon2_id = 2
// } argon2_type;
const argon2idType = 2;
const version = 0x13;
const saltLength = 12;

class Benchmark {
    async init() {
        Module.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
    }

    async runIteration() {
        // Instantiate the Wasm module before the first run.
        if (!Module._argon2_hash) {
            await setupModule(Module);
        }

        for (let i = 0; i < passwordStrings.length; ++i)
            this.hashAndVerify(passwordStrings[i]);
    }

    randomSalt() {
        let result = new MallocPtr(saltLength);
        const numWords = saltLength / 4;
        for (let i = 0; i < numWords; ++i)
            Module.HEAPU32[result.ptr + i] = Math.floor(Math.random() * (2 ** 32));
        return result;
    }

    hashAndVerify(password) {
        password = new CString(password);
        let salt = this.randomSalt();
        let hashBuffer = new MallocPtr(24);
        let encodedBuffer = new MallocPtr(Module._argon2_encodedlen(tCost, mCost, parallelism, salt.size, hashBuffer.size, argon2idType) + 1);

        let status = Module._argon2_hash(tCost, mCost, parallelism, password.ptr, password.length, salt.ptr, salt.size, hashBuffer.ptr, hashBuffer.size, encodedBuffer.ptr, encodedBuffer.size, argon2idType, version);
        if (status !== 0)
            throw new Error(`argon2_hash exited with status: ${status} (${Module.UTF8ToString(Module._argon2_error_message(status))})`);

        status = Module._argon2_verify(encodedBuffer.ptr, password.ptr, password.length, argon2idType);
        if (status !== 0)
            throw new Error(`argon2_verify exited with status: ${status} (${Module.UTF8ToString(Module._argon2_error_message(status))})`);

        password.dispose();
        salt.dispose();
        hashBuffer.dispose();
        encodedBuffer.dispose();
    }
}
