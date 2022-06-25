// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetimeiso
description: RangeError thrown if time zone reports an offset that is out of range
features: [Temporal, arrow-function]
includes: [temporalHelpers.js]
---*/
[-86400000000001, 86400000000001, -Infinity, Infinity].forEach(wrongOffset => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);

  assert.throws(
    RangeError,
    () => Temporal.Now.plainDateTimeISO(timeZone),
    'Temporal.Now.plainDateTimeISO(timeZone) throws a RangeError exception'
  );
});
