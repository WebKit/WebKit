// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
class base { constructor() { } }

// lies and the lying liars who tell them
function lies() { }
lies.prototype = 4;

assertThrowsInstanceOf(()=>Reflect.consruct(base, [], lies), TypeError);

// lie a slightly different way
function get(target, property, receiver) {
    if (property === "prototype")
        return 42;
    return Reflect.get(target, property, receiver);
}

class inst extends base {
    constructor() { super(); }
}
assertThrowsInstanceOf(()=>new new Proxy(inst, {get})(), TypeError);

class defaultInst extends base {}
assertThrowsInstanceOf(()=>new new Proxy(defaultInst, {get})(), TypeError);

