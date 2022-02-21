// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetimeiso
description: Correctly invokes `getOffsetNanosecondsFor` method of TimeZone-like objects
features: [Temporal]
---*/

var calls = [];
var timeZone = {
  getOffsetNanosecondsFor: function() {
    calls.push({
      args: arguments,
      this: this
    });
    return 0;
  },
};

Temporal.Now.plainDateTimeISO(timeZone);

assert.sameValue(calls.length, 1, 'The value of calls.length is expected to be 1');
assert.sameValue(calls[0].args.length, 1, 'The value of calls[0].args.length is expected to be 1');
assert(
  calls[0].args[0] instanceof Temporal.Instant,
  'The result of evaluating (calls[0].args[0] instanceof Temporal.Instant) is expected to be true'
);
assert.sameValue(calls[0].this, timeZone, 'The value of calls[0].this is expected to equal the value of timeZone');
