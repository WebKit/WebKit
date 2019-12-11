// Copyright (C) 2019 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-json.stringify
description: >
  Tests that JSON.stringify meets the requirements
  for built-in objects defined by the introduction of chapter 17 of
  the ECMAScript Language Specification.
includes: [isConstructor.js]
features: [Reflect.construct]
---*/

assert(Object.isExtensible(JSON.stringify));
assert.sameValue(Object.prototype.toString.call(JSON.stringify), '[object Function]');
assert.sameValue(Object.getPrototypeOf(JSON.stringify), Function.prototype);
assert.sameValue(JSON.stringify.hasOwnProperty('prototype'), false);
assert.sameValue(isConstructor(JSON.stringify), false);
