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

export const notUndef = (v) => {
    if (v === undefined)
        throw new Error("Shouldn't be undefined");
};

export const eq = (lhs, rhs) => {
    if (lhs !== rhs)
        throw new Error(`Not the same: "${lhs}" and "${rhs}"`);
};

export const ge = (lhs, rhs) => {
    notUndef(lhs);
    notUndef(rhs);
    if (!(lhs >= rhs))
        throw new Error(`Expected: "${lhs}" < "${rhs}"`);
};

export const throwsError = (opFn, message, ...args) => {
    if (message)
        message = " for " + message;

    try {
        opFn(...args);
    } catch (e) {
        if (e instanceof Error)
            return;
        throw new Error(`Expected an Error${message}, got ${e}`);
    }
    throw new Error(`Expected to throw an Error${message}`);
};

export const throwsRangeError = (opFn, message, ...args) => {
    if (message)
        message = " for " + message;

    try {
        opFn(...args);
    } catch (e) {
        if (e instanceof RangeError)
            return;
        throw new Error(`Expected a RangeError${message}, got ${e}`);
    }
    throw new Error(`Expected to throw a RangeError${message}`);
};
