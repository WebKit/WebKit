// Copyright (C) 2019 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-json.parse
description: >
  Requirements for built-in functions, defined in introduction of chapter 17,
  are satisfied.
includes: [isConstructor.js]
features: [Reflect.construct]
---*/

var parse = JSON.parse;
assert(Object.isExtensible(parse));
assert.sameValue(typeof parse, 'function');
assert.sameValue(Object.prototype.toString.call(parse), '[object Function]');
assert.sameValue(Object.getPrototypeOf(parse), Function.prototype);

assert.sameValue(parse.hasOwnProperty('prototype'), false);
assert.sameValue(isConstructor(parse), false);
