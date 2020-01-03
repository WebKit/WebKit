// Copyright (C) 2009 the Sputnik authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.prototype.isprototypeof
description: >
  Tests that Object.prototype.isPrototypeOf meets the requirements
  for built-in objects defined by the introduction of chapter 17 of
  the ECMAScript Language Specification.
includes: [isConstructor.js]
features: [Reflect.construct]
---*/

assert(Object.isExtensible(Object.prototype.isPrototypeOf));
assert.sameValue(Object.prototype.toString.call(Object.prototype.isPrototypeOf), "[object Function]");
assert.sameValue(Object.getPrototypeOf(Object.prototype.isPrototypeOf), Function.prototype);
assert.sameValue(Object.prototype.isPrototypeOf.hasOwnProperty("prototype"), false);
assert.sameValue(isConstructor(Object.prototype.isPrototypeOf), false);
