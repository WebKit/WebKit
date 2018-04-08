// Copyright (C) 2018 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-function.prototype.tostring
description: Function.prototype.toString on anonymous well-known intrinsic function objects
includes: [nativeFunctionMatcher.js]
---*/

var ThrowTypeError = (function() { "use strict"; return Object.getOwnPropertyDescriptor(arguments, "callee").get })()
assert.sameValue(ThrowTypeError.name, "");
assertNativeFunction(ThrowTypeError);
