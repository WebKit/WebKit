// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetimeiso
description: RangeError thrown if time zone reports an offset that is not an integer number of nanoseconds
features: [Temporal, arrow-function]
includes: [temporalHelpers.js]
---*/
[3600000000000.5, NaN].forEach(wrongOffset => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);

  assert.throws(
    RangeError,
    () => Temporal.Now.plainDateTimeISO(timeZone),
    'Temporal.Now.plainDateTimeISO(timeZone) throws a RangeError exception'
  );
});
