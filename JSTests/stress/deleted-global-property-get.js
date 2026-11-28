/*
 * Copyright (C) 2026 Sergey Rubanov. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

function assertThrowsReferenceError(fn) {
    try {
        fn();
    } catch (e) {
        if (e instanceof ReferenceError)
            return;
        throw e;
    }
    throw new Error("did not throw");
}

globalThis.gv = 1;
function readDeleted() { return gv; }
noInline(readDeleted);
for (let i = 0; i < testLoopCount; ++i) {
    if (readDeleted() !== 1)
        throw new Error("warmup");
}
if (!delete globalThis.gv)
    throw new Error("delete");
for (let i = 0; i < testLoopCount; ++i)
    assertThrowsReferenceError(readDeleted);

globalThis.gUndef = undefined;
function readUndefined() { return gUndef; }
noInline(readUndefined);
for (let i = 0; i < testLoopCount; ++i) {
    if (readUndefined() !== undefined)
        throw new Error("undefined value");
}
if (!delete globalThis.gUndef)
    throw new Error("delete undefined");
for (let i = 0; i < testLoopCount; ++i)
    assertThrowsReferenceError(readUndefined);

globalThis.gProto = 1;
function readPrototype() { return gProto; }
noInline(readPrototype);
for (let i = 0; i < testLoopCount; ++i) {
    if (readPrototype() !== 1)
        throw new Error("prototype warmup");
}
if (!delete globalThis.gProto)
    throw new Error("delete prototype");
Object.prototype.gProto = 9;
for (let i = 0; i < testLoopCount; ++i) {
    if (readPrototype() !== 9)
        throw new Error("prototype");
}
delete Object.prototype.gProto;

let getterCalls = 0;
Object.defineProperty(globalThis, "gGet", {
    configurable: true,
    get() {
        getterCalls++;
        return undefined;
    },
});
function readGetter() { return gGet; }
noInline(readGetter);
for (let i = 0; i < testLoopCount; ++i) {
    if (readGetter() !== undefined)
        throw new Error("getter");
}
if (getterCalls !== testLoopCount)
    throw new Error("getter calls " + getterCalls);
if (!delete globalThis.gGet)
    throw new Error("delete getter");
for (let i = 0; i < testLoopCount; ++i)
    assertThrowsReferenceError(readGetter);

globalThis.gType = 1;
function typeOfBinding() { return typeof gType; }
noInline(typeOfBinding);
for (let i = 0; i < testLoopCount; ++i) {
    if (typeOfBinding() !== "number")
        throw new Error("typeof warmup");
}
if (!delete globalThis.gType)
    throw new Error("delete typeof");
for (let i = 0; i < testLoopCount; ++i) {
    if (typeOfBinding() !== "undefined")
        throw new Error("typeof");
}

globalThis.gStrict = 1;
function readStrict() { "use strict"; return gStrict; }
noInline(readStrict);
for (let i = 0; i < testLoopCount; ++i) {
    if (readStrict() !== 1)
        throw new Error("strict warmup");
}
if (!delete globalThis.gStrict)
    throw new Error("delete strict");
for (let i = 0; i < testLoopCount; ++i)
    assertThrowsReferenceError(readStrict);

globalThis.gStrictSet = 1;
function strictSet() { "use strict"; gStrictSet = 2; }
noInline(strictSet);
for (let i = 0; i < testLoopCount; ++i)
    strictSet();
if (globalThis.gStrictSet !== 2)
    throw new Error("strict set warmup");
if (!delete globalThis.gStrictSet)
    throw new Error("delete strict set");
for (let i = 0; i < testLoopCount; ++i)
    assertThrowsReferenceError(strictSet);
if ("gStrictSet" in globalThis)
    throw new Error("strict set created the binding");

globalThis.gSloppySet = 1;
function sloppySet() { gSloppySet = 3; }
noInline(sloppySet);
for (let i = 0; i < testLoopCount; ++i)
    sloppySet();
if (!delete globalThis.gSloppySet)
    throw new Error("delete sloppy set");
for (let i = 0; i < testLoopCount; ++i)
    sloppySet();
if (globalThis.gSloppySet !== 3)
    throw new Error("sloppy set");

globalThis.gShadow = 1;
function strictShadow() { "use strict"; gShadow = 4; }
noInline(strictShadow);
for (let i = 0; i < testLoopCount; ++i)
    strictShadow();
if (!delete globalThis.gShadow)
    throw new Error("delete shadow");
Object.prototype.gShadow = 5;
for (let i = 0; i < testLoopCount; ++i)
    strictShadow();
if (globalThis.gShadow !== 4)
    throw new Error("shadow");
delete globalThis.gShadow;
delete Object.prototype.gShadow;
