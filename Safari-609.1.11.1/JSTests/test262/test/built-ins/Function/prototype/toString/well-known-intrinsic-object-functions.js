// Copyright (C) 2016 Michael Ficarra. All rights reserved.
// Copyright (C) 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-function.prototype.tostring
description: >
  toString of Well-Known Intrinsic Object
info: |
  ...
  If func is a Bound Function exotic object or a built-in Function object, then return an implementation-dependent String source code representation of func. The representation must have the syntax of a NativeFunction. Additionally, if func is a Well-known Intrinsic Object and is not identified as an anonymous function, the portion of the returned String that would be matched by IdentifierName must be the initial value of the name property of func.

  NativeFunction:
    function IdentifierName_opt ( FormalParameters ) { [ native code ] }

includes: [nativeFunctionMatcher.js, wellKnownIntrinsicObjects.js]
features: [destructuring-binding, template]
---*/


WellKnownIntrinsicObjects.forEach(({intrinsicName, reference}) => {
  if (typeof reference === "function") {
    assert.sameValue(
      ("" + reference).includes(reference.name), true,
      `toString of Well-Known Intrinsic Object function should produce a string that matches NativeFunction syntax:\n${reference}\nis missing "${reference.name}"\n\n`
    );
    assertNativeFunction(reference, intrinsicName);
  }
});
