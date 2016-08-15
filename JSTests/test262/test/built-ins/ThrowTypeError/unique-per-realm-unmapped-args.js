// Copyright (C) 2016 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%throwtypeerror%
description: >
  %ThrowTypeError% is defined once for each realm.
info: >
  %ThrowTypeError% ( )

  The %ThrowTypeError% intrinsic is an anonymous built-in function
  object that is defined once for each realm.
---*/

var ThrowTypeError = Object.getOwnPropertyDescriptor(function(){ "use strict"; return arguments; }(), "callee").get;

// Test with unmapped arguments object from strict-mode function.
function strictFn() {
  "use strict";
  return arguments;
}
var unmappedCalleeDesc = Object.getOwnPropertyDescriptor(strictFn(), "callee");
var unmappedCallerDesc = Object.getOwnPropertyDescriptor(strictFn(), "caller");

assert.sameValue(ThrowTypeError, unmappedCalleeDesc.get, "callee.get");
assert.sameValue(ThrowTypeError, unmappedCalleeDesc.set, "callee.set");
assert.sameValue(ThrowTypeError, unmappedCallerDesc.get, "caller.get");
assert.sameValue(ThrowTypeError, unmappedCallerDesc.set, "caller.set");
