// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetime
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

Temporal.Now.plainDateTime('iso8601', timeZone);

assert.sameValue(calls.length, 1, 'call count');
assert.sameValue(calls[0].args.length, 1, 'arguments');
assert(calls[0].args[0] instanceof Temporal.Instant);
assert.sameValue(calls[0].this, timeZone);
