// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.tozoneddatetime
description: TypeError thrown if a primitive is passed as the argument
info: |
    Temporal.PlainTime.prototype.toZonedDateTime ( item )

    3. If Type(item) is not Object, then
        a. Throw a TypeError exception.
features: [Symbol, Temporal]
---*/

const instance = Temporal.PlainTime.from("00:00");

assert.throws(TypeError, () => instance.toZonedDateTime(undefined), "undefined");
assert.throws(TypeError, () => instance.toZonedDateTime(null), "null");
assert.throws(TypeError, () => instance.toZonedDateTime(true), "true");
assert.throws(TypeError, () => instance.toZonedDateTime(""), "empty string");
assert.throws(TypeError, () => instance.toZonedDateTime(Symbol()), "symbol");
assert.throws(TypeError, () => instance.toZonedDateTime(1), "1");
assert.throws(TypeError, () => instance.toZonedDateTime(1n), "1n");
