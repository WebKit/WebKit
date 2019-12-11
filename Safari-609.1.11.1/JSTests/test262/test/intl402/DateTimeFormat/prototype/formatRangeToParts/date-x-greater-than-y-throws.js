// Copyright 2019 Google, Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Throws a RangeError if date x is greater than y.
info: |
  Intl.DateTimeFormat.prototype.formatRangeToParts ( startDate , endDate )

  1. Let dtf be this value.
  2. If Type(dtf) is not Object, throw a TypeError exception.
  3. If dtf does not have an [[InitializedDateTimeFormat]] internal slot, throw a TypeError exception.
  5. Let x be ? ToNumber(startDate).
  6. Let y be ? ToNumber(endDate).
  7. If x is greater than y, throw a RangeError exception.

features: [Intl.DateTimeFormat-formatRange]
---*/

var dtf = new Intl.DateTimeFormat();

var x = new Date();
var y = new Date();
x.setDate(y.getDate() + 1);

assert.throws(RangeError, function() {
  dtf.formatRangeToParts(x, y);
}, "x > y");
assert.sameValue("object", typeof dtf.formatRangeToParts(x, x));
assert.sameValue("object", typeof dtf.formatRangeToParts(y, y));
assert.sameValue("object", typeof dtf.formatRangeToParts(y, x));
