// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Forwards error thrown by invoking "toString" property
features: [Temporal]
---*/

var calendar = {
  calendar: {
    calendar: true,
    toString: function() {
      throw new Test262Error();
    },
  }
};

assert.throws(Test262Error, function() {
  Temporal.Now.zonedDateTime(calendar);
}, 'Temporal.Now.zonedDateTime(calendar) throws a Test262Error exception');
