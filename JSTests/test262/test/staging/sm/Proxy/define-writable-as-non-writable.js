// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
flags:
  - onlyStrict
includes: [sm/non262.js, sm/non262-shell.js]
description: |
  pending
esid: pending
---*/
"use strict";

var target = {};
Object.defineProperty(target, "test", {configurable: false, writable: true, value: 5});

var proxy = new Proxy(target, {
    defineProperty(target, property) {
        assert.sameValue(property, "test");
        return true;
    }
});

assertThrowsInstanceOf(
    () => Object.defineProperty(proxy, "test", {writable: false}), TypeError);

assertThrowsInstanceOf(
    () => Reflect.defineProperty(proxy, "test", {writable: false}), TypeError);

