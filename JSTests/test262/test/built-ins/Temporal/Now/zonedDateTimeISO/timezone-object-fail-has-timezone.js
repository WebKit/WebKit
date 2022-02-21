// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetimeiso
description: Forwards error thrown by checking presence of "timeZone" property
features: [Proxy, Temporal]
---*/

var timeZone = new Proxy({}, {
  has: function(target, property) {
    if (property === 'timeZone') {
      throw new Test262Error();
    }
  },
});

assert.throws(Test262Error, function() {
  Temporal.Now.zonedDateTimeISO(timeZone);
}, 'Temporal.Now.zonedDateTimeISO(timeZone) throws a Test262Error exception');
