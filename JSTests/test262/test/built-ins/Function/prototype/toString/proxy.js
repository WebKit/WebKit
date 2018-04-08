// Copyright (C) 2018 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-function.prototype.tostring
description: Function.prototype.toString on a proxied function
info: |
  Function.prototype.toString accepts any callable, including proxied
  functions, and produces a NativeFunction
includes: [nativeFunctionMatcher.js]
---*/

const f = new Proxy(function(){}, {});
assertNativeFunction(f);

const g = new Proxy(f, { apply() {} });
assertNativeFunction(g);
