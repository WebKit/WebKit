// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var names = Object.getOwnPropertyNames(Object.getOwnPropertyDescriptor({foo: 0}, "foo"));
assert.deepEqual(names, ["value", "writable", "enumerable", "configurable"]);

names = Object.getOwnPropertyNames(Object.getOwnPropertyDescriptor({get foo(){}}, "foo"));
assert.deepEqual(names, ["get", "set", "enumerable", "configurable"]);

var proxy = new Proxy({}, {
    defineProperty(target, key, desc) {
        var names = Object.getOwnPropertyNames(desc);
        assert.deepEqual(names, ["set", "configurable"]);
        return true;
    }
});

Object.defineProperty(proxy, "foo", {configurable: true, set: function() {}});

