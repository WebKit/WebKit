// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Forwards error thrown by retrieving value of "calendar" property
features: [Temporal]
---*/

var calendar = {
  get calendar() {
    throw new Test262Error();
  },
};

assert.throws(Test262Error, function() {
  Temporal.Now.zonedDateTime(calendar);
}, 'Temporal.Now.zonedDateTime(calendar) throws a Test262Error exception');
