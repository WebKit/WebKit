// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetime
description: Forwards error thrown by checking presence of nested "calendar" property
features: [Proxy, Temporal]
---*/

var calendar = {
  calendar: new Proxy({}, {
    has: function(target, property) {
      if (property === 'calendar') {
        throw new Test262Error();
      }
    },
  })
};

assert.throws(Test262Error, function() {
  Temporal.Now.plainDateTime(calendar);
}, 'Temporal.Now.plainDateTime(calendar) throws a Test262Error exception');
