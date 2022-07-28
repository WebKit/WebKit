// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
  Throws a RangeError if "relativeTo" is a date/time value outside the valid limits.
info: |
  Temporal.Duration.prototype.total ( totalOf )
  ...
  6. Let relativeTo be ? ToRelativeTemporalObject(totalOf).
  ...

  ToRelativeTemporalObject ( options )
  ...
  9. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], calendar).
features: [Temporal]
---*/

var duration = Temporal.Duration.from({nanoseconds: 0});
var options = {unit: "nanoseconds", relativeTo: "+999999-01-01"};

assert.throws(RangeError, () => duration.total(options));
