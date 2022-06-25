// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.8
description: >
    Throws if proxy return has not the same value for a non-writable,
    non-configurable property
info: |
    [[Get]] (P, Receiver)

    13. If targetDesc is not undefined, then
        a. If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is
        false and targetDesc.[[Writable]] is false, then
            i. If SameValue(trapResult, targetDesc.[[Value]]) is false, throw a
            TypeError exception.
features: [Proxy]
---*/

var target = {};
var p = new Proxy(target, {
  get: function() {
    return 2;
  }
});

Object.defineProperty(target, 'attr', {
  configurable: false,
  writable: false,
  value: 1
});

assert.throws(TypeError, function() {
  p.attr;
});

assert.throws(TypeError, function() {
  p['attr'];
});
