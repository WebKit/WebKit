// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetime
description: Forwards error thrown by checking presence of "calendar" property
features: [Proxy, Temporal]
---*/

var calendar = new Proxy({}, {
  has: function(target, property) {
    if (property === 'calendar') {
      throw new Test262Error();
    }
  },
});

assert.throws(Test262Error, function() {
  Temporal.Now.zonedDateTime(calendar);
}, 'Temporal.Now.zonedDateTime(calendar) throws a Test262Error exception');
