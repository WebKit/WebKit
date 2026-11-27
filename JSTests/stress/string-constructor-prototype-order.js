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

function newTarget(prototype, log) {
    return new Proxy(function() { }, {
        get(target, key, receiver) {
            log.push("get " + String(key));
            if (key === "prototype") {
                if (prototype === null)
                    throw 0;
                return prototype;
            }
            return Reflect.get(target, key, receiver);
        },
    });
}

{
    let log = [];
    let value = {
        [Symbol.toPrimitive](hint) {
            log.push("toPrimitive " + hint);
            return "ab";
        },
    };
    let proto = {};
    let result = Reflect.construct(String, [value], newTarget(proto, log));
    if (log.join(",") !== "toPrimitive string,get prototype")
        throw new Error("order " + log.join(","));
    if (Object.getPrototypeOf(result) !== proto)
        throw new Error("prototype");
    if (String.prototype.valueOf.call(result) !== "ab")
        throw new Error("value " + result);
}

{
    let log = [];
    let value = {
        [Symbol.toPrimitive]() {
            throw new Error("prim");
        },
    };
    try {
        Reflect.construct(String, [value], newTarget({}, log));
        throw new Error("missing throw");
    } catch (e) {
        if (String(e) !== "Error: prim")
            throw e;
    }
    if (log.length)
        throw new Error("prototype read " + log.join(","));
}

{
    let log = [];
    let value = {
        [Symbol.toPrimitive](hint) {
            log.push("toPrimitive " + hint);
            return "ab";
        },
    };
    try {
        Reflect.construct(String, [value], newTarget(null, log));
        throw new Error("missing prototype throw");
    } catch (e) {
        if (e !== 0)
            throw e;
    }
    if (log.join(",") !== "toPrimitive string,get prototype")
        throw new Error("throw order " + log.join(","));
}

{
    let log = [];
    try {
        Reflect.construct(String, [Symbol.iterator], newTarget({}, log));
        throw new Error("missing symbol throw");
    } catch (e) {
        if (!(e instanceof TypeError))
            throw e;
    }
    if (log.length)
        throw new Error("symbol read " + log.join(","));
}

{
    let log = [];
    let proto = {};
    let result = Reflect.construct(String, [], newTarget(proto, log));
    if (log.join(",") !== "get prototype")
        throw new Error("empty " + log.join(","));
    if (Object.getPrototypeOf(result) !== proto)
        throw new Error("empty prototype");
    if (String.prototype.valueOf.call(result) !== "")
        throw new Error("empty value");
}

{
    let log = [];
    let value = {
        [Symbol.toPrimitive](hint) {
            log.push(hint);
            return "z";
        },
    };
    if (String(value) !== "z")
        throw new Error("call");
    if (log.join(",") !== "string")
        throw new Error("call order " + log.join(","));
}
