// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.5.6.3
description: >
  The prototype of EvalError.prototype is Error.prototype.
info: |
  The value of the [[Prototype]] internal slot of each NativeError prototype
  object is the intrinsic object %ErrorPrototype% (19.5.3).
---*/

assert.sameValue(Object.getPrototypeOf(EvalError.prototype), Error.prototype);
