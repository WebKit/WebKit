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

// Portable environment: one of ["node", "web", "worker", "shell"].
const _environment =
    ((typeof process === "object" && typeof require === "function") ? "node"
     : (typeof window === "object" ? "web"
        : (typeof importScripts === "function" ? "worker"
           : "shell")));

let _global = (typeof global !== 'object' || !global || global.Math !== Math || global.Array !== Array)
    ? ((typeof self !== 'undefined') ? self
       : (typeof window !== 'undefined') ? window
       : (typeof global !== 'undefined') ? global
       : Function('return this')())
    : global;

const _eval = x => eval.call(null, x);

const _read = filename => {
    switch (_environment) {
    case "node":   return read(filename);
    case "web":    // fallthrough
    case "worker": let xhr = new XMLHttpRequest(); xhr.open("GET", filename, /*async=*/false); return xhr.responseText;
    case "shell":  return read(filename);
    }
}

const _load = filename => {
    switch (_environment) {
    case "node":   // fallthrough
    case "web":    // fallthrough
    case "shell":  return _eval(_read(filename));
    case "worker": return importScripts(filename);
    }
}

const _json = filename => {
    switch (_environment) {
    case "node":   // fallthrough
    case "shell":  return JSON.parse(_read(filename));
    case "web":    // fallthrough
    case "worker": let xhr = new XMLHttpRequest(); xhr.overrideMimeType("application/json"); xhr.open("GET", filename, /*async=*/false); return xhr.response;
    }
}

const _dump = (what, name, pad = '    ') => {
    const value = v => {
        try { return `"${v}"`; }
        catch (e) { return `Error: "${e.message}"`; }
    };
    let s = `${pad}${name} ${typeof what}: ${value(what)}`;
    for (let p in what) {
        s += `\n${pad}${pad}${p}: ${value(what[p])} ${typeof v}`;
        s += '\n' + _dump(what[p], p, pad + pad);
    }
    return s;
};

// Use underscore names to avoid clashing with builtin names.
export {
    _dump as dump,
    _eval as eval,
    _read as read,
    _load as load,
    _json as json,
    _global as global
};
