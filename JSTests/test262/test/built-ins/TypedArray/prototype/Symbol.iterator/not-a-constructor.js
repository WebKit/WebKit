// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-ecmascript-standard-built-in-objects
description: >
  TypedArray.prototype[Symbol.iterator] does not implement [[Construct]], is not new-able
info: |
  ECMAScript Function Objects

  Built-in function objects that are not identified as constructors do not
  implement the [[Construct]] internal method unless otherwise specified in
  the description of a particular function.

  sec-evaluatenew

  ...
  7. If IsConstructor(constructor) is false, throw a TypeError exception.
  ...
includes: [isConstructor.js, testTypedArray.js]
features: [Reflect.construct, Symbol, Symbol.iterator, arrow-function, TypedArray]
---*/

assert.sameValue(
  isConstructor(TypedArray.prototype[Symbol.iterator]),
  false,
  'isConstructor(TypedArray.prototype[Symbol.iterator]) must return false'
);

assert.throws(TypeError, () => {
  let u8 = new Uint8Array(1); new u8[Symbol.iterator]();
}, '`let u8 = new Uint8Array(1); new u8[Symbol.iterator]()` throws TypeError');

