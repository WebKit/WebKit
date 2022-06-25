// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetime
description: TypeError thrown if time zone reports an offset that is not a Number
features: [BigInt, Symbol, Temporal, arrow-function]
includes: [temporalHelpers.js]
---*/
[undefined, null, true, '+01:00', Symbol(), 3600000000000n, {}, {
  valueOf() {
    return 3600000000000;
  }
}].forEach(wrongOffset => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);

  assert.throws(
    TypeError,
    () => Temporal.Now.plainDateTime('iso8601', timeZone),
    'Temporal.Now.plainDateTime("iso8601", timeZone) throws a TypeError exception'
  );
});
