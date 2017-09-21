// Copyright (C) 2017 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
author: Jordan Harband
description: finally on a fulfilled promise can not override the resolution value
esid: sec-promise.prototype.finally
features: [Promise.prototype.finally]
flags: [async]
---*/

var obj = {};

var p = Promise.resolve(obj);

p.finally(function () {
  assert.sameValue(arguments.length, 0, 'onFinally receives zero args');
  return {};
}).then(function (x) {
  assert.sameValue(x, obj, 'onFinally can not override the resolution value');
}).then($DONE).catch($ERROR);
