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

const _notUndef = (v) => {
    if (typeof v === "undefined")
        throw new Error("Shouldn't be undefined");
};

const _isUndef = (v) => {
    if (typeof v !== "undefined")
        throw new Error("Should be undefined");
};

const _eq = (lhs, rhs) => {
    if (lhs !== rhs)
        throw new Error(`Not the same: "${lhs}" and "${rhs}"`);
};

const _ge = (lhs, rhs) => {
    _notUndef(lhs);
    _notUndef(rhs);
    if (!(lhs >= rhs))
        throw new Error(`Expected: "${lhs}" < "${rhs}"`);
};

const _throws = (func, type, message, ...args) => {
    try {
        func(...args);
    } catch (e) {
        if (e instanceof type && e.message === message)
            return;
        throw new Error(`Expected to throw a ${type.name} with message "${message}", got ${e.name} with message "${e.message}"`);
    }
    throw new Error(`Expected to throw a ${type.name} with message "${message}"`);
};

const _instanceof = (obj, type) => obj instanceof type;

// Use underscore names to avoid clashing with builtin names.
export {
    _notUndef as notUndef,
    _isUndef as isUndef,
    _eq as eq,
    _ge as ge,
    _throws as throws,
    _instanceof as instanceof,
};
