// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plainDateTime
description: Temporal.Now.plainDateTime.name is "plainDateTime".
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  Temporal.Now.plainDateTime.name,
  'plainDateTime',
  'The value of Temporal.Now.plainDateTime.name is expected to be "plainDateTime"'
);

verifyProperty(Temporal.Now.plainDateTime, 'name', {
  enumerable: false,
  writable: false,
  configurable: true
});
