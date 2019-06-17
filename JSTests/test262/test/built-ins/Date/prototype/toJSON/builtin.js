// Copyright (C) 2019 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype.tojson
description: >
  Tests that Date.prototype.toJSON meets the requirements
  for built-in objects defined by the introduction of chapter 17 of
  the ECMAScript Language Specification.
includes: [isConstructor.js]
features: [Reflect.construct]
---*/

var toJSON = Date.prototype.toJSON;

assert(Object.isExtensible(toJSON));
assert.sameValue(typeof toJSON, 'function');
assert.sameValue(Object.prototype.toString.call(toJSON), '[object Function]');
assert.sameValue(Object.getPrototypeOf(toJSON), Function.prototype);

assert.sameValue(toJSON.hasOwnProperty('prototype'), false);
assert.sameValue(isConstructor(toJSON), false);
