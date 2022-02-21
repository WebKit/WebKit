// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetimeiso
description: Rejects when `getOffsetNanosecondsFor` property is not a method
features: [Temporal]
---*/

var timeZone = {
  getOffsetNanosecondsFor: 7
};

assert.throws(TypeError, function() {
  Temporal.Now.plainDateTimeISO(timeZone);
}, 'Temporal.Now.plainDateTimeISO(timeZone) throws a TypeError exception');
