// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.every
description: >
    Array.prototype.every applied to Array-like object, 'length' is an
    own data property that overrides an inherited accessor property
---*/

function callbackfn1(val, idx, obj) {
  return val > 10;
}

function callbackfn2(val, idx, obj) {
  return val > 11;
}

var proto = {};

Object.defineProperty(proto, "length", {
  get: function() {
    return 3;
  },
  configurable: true
});

var Con = function() {};
Con.prototype = proto;

var child = new Con();
child[0] = 12;
child[1] = 11;
child[2] = 9;

Object.defineProperty(child, "length", {
  value: 2,
  configurable: true
});

assert(Array.prototype.every.call(child, callbackfn1), 'Array.prototype.every.call(child, callbackfn1) !== true');
assert.sameValue(Array.prototype.every.call(child, callbackfn2), false, 'Array.prototype.every.call(child, callbackfn2)');
