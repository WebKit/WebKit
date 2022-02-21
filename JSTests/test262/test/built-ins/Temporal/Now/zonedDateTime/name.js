// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Temporal.Now.zonedDateTime.name is "zonedDateTime".
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  Temporal.Now.zonedDateTime.name,
  'zonedDateTime',
  'The value of Temporal.Now.zonedDateTime.name is expected to be "zonedDateTime"'
);

verifyProperty(Temporal.Now.zonedDateTime, 'name', {
  enumerable: false,
  writable: false,
  configurable: true
});
