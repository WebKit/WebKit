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

const _fail = (msg, extra) => {
    throw new Error(msg + (extra ? ": " + extra : ""));
};

export const isNotA = (v, t, msg) => {
    if (typeof v === t)
        _fail(`Shouldn't be ${t}`, msg);
};

export const isA = (v, t, msg) => {
    if (typeof v !== t)
        _fail(`Should be ${t}, got ${typeof(v)}`, msg);
};

export const isNotUndef = (v, msg) => isNotA(v, "undefined", msg);
export const isUndef = (v, msg) => isA(v, "undefined", msg);
export const notObject = (v, msg) => isNotA(v, "object", msg);
export const isObject = (v, msg) => isA(v, "object", msg);
export const notString = (v, msg) => isNotA(v, "string", msg);
export const isString = (v, msg) => isA(v, "string", msg);
export const notNumber = (v, msg) => isNotA(v, "number", msg);
export const isNumber = (v, msg) => isA(v, "number", msg);

export const hasObjectProperty = (o, p, msg) => {
    isObject(o, msg);
    isNotUndef(o[p], msg, `expected object to have property ${p}`);
};

export const isArray = (v, msg) => {
    if (!Array.isArray(v))
        _fail(`Expected an array, got ${typeof(v)}`, msg);
};

export const isNotArray = (v, msg) => {
    if (Array.isArray(v))
        _fail(`Expected to not be an array`, msg);
};

export const truthy = (v, msg) => {
    if (!v)
        _fail(`Expected truthy`, msg);
};

export const falsy = (v, msg) => {
    if (v)
        _fail(`Expected falsy`, msg);
};

export const eq = (lhs, rhs, msg) => {
    if (typeof lhs !== typeof rhs)
        _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    if (Array.isArray(lhs) && Array.isArray(rhs) && (lhs.length === rhs.length)) {
        for (let i = 0; i !== lhs.length; ++i)
            eq(lhs[i], rhs[i], msg);
    } else if (lhs !== rhs) {
        if (typeof lhs === "number" && isNaN(lhs) && isNaN(rhs))
            return;
        _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    } else {
        if (typeof lhs === "number" && (1.0 / lhs !== 1.0 / rhs)) // Distinguish -0.0 from 0.0.
            _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    }
};

const canonicalizeI32 = (number) => {
    if (Math.round(number) === number && number >= 2 ** 31)
        number = number - 2 ** 32;
    return number;
}

export const eqI32 = (lhs, rhs, msg) => {
    return eq(canonicalizeI32(lhs), canonicalizeI32(rhs), msg);
};

export const ge = (lhs, rhs, msg) => {
    isNotUndef(lhs);
    isNotUndef(rhs);
    if (!(lhs >= rhs))
        _fail(`Expected: "${lhs}" < "${rhs}"`, msg);
};

export const le = (lhs, rhs, msg) => {
    isNotUndef(lhs);
    isNotUndef(rhs);
    if (!(lhs <= rhs))
        _fail(`Expected: "${lhs}" > "${rhs}"`, msg);
};

const _throws = (func, type, message, ...args) => {
    try {
        func(...args);
    } catch (e) {
        if (e instanceof type && e.message === message)
            return;
        _fail(`Expected to throw a ${type.name} with message "${message}", got ${e.name} with message "${e.message}"`);
    }
    _fail(`Expected to throw a ${type.name} with message "${message}"`);
};

const _instanceof = (obj, type, msg) => {
    if (!(obj instanceof type))
        _fail(`Expected a ${typeof(type)}, got ${typeof obj}`);
};

// Use underscore names to avoid clashing with builtin names.
export {
    _throws as throws,
    _instanceof as instanceof,
};
