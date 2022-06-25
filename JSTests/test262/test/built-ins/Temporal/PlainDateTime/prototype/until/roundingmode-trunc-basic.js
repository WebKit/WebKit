// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: A variety of tests for truncation (trunc) round mode
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainDateTime(2019, 1, 8, 8, 22, 36, 123, 456, 789);
const later = new Temporal.PlainDateTime(2021, 9, 7, 12, 39, 40, 987, 654, 321);

function ensureUnsignedZero(x) {
  return Object.is(x, -0) ? 0 : x;
}

const incrementOneTrunc = [
  ["years", [2]],
  ["months", [0, 31]],
  ["weeks", [0, 0, 139]],
  ["days", [0, 0, 0, 973]],
  ["hours", [0, 0, 0, 973, 4]],
  ["minutes", [0, 0, 0, 973, 4, 17]],
  ["seconds", [0, 0, 0, 973, 4, 17, 4]],
  ["milliseconds", [0, 0, 0, 973, 4, 17, 4, 864]],
  ["microseconds", [0, 0, 0, 973, 4, 17, 4, 864, 197]],
  ["nanoseconds", [0, 0, 0, 973, 4, 17, 4, 864, 197, 532]]
];
incrementOneTrunc.forEach(([smallestUnit, expected]) => {
  const [y, m = 0, w = 0, d = 0, h = 0, min = 0, s = 0, ms = 0, µs = 0, ns = 0] = expected;
  TemporalHelpers.assertDuration(
    earlier.until(later, {smallestUnit, roundingMode: "trunc"}),
    y, m, w, d, h, min, s, ms, µs, ns,
    `truncates to ${smallestUnit} (rounding mode = trunc, positive case)`
  );
  TemporalHelpers.assertDuration(
    later.until(earlier, {smallestUnit, roundingMode: "trunc"}),
    ensureUnsignedZero(-y),
    ensureUnsignedZero(-m),
    ensureUnsignedZero(-w),
    ensureUnsignedZero(-d),
    ensureUnsignedZero(-h),
    ensureUnsignedZero(-min),
    ensureUnsignedZero(-s),
    ensureUnsignedZero(-ms),
    ensureUnsignedZero(-µs),
    ensureUnsignedZero(-ns),
    `truncates to ${smallestUnit} (rounding mode = trunc, negative case)`
  );
});
