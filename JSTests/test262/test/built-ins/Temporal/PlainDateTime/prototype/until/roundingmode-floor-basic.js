// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: Checking that floor rounding mode rounds correctly
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const earlier = new Temporal.PlainDateTime(2019, 1, 8, 8, 22, 36, 123, 456, 789);
const later = new Temporal.PlainDateTime(2021, 9, 7, 12, 39, 40, 987, 654, 321);

const incrementOneFloor = [
  ["years", [2], [-3]],
  ["months", [0, 31], [0, -32]],
  ["weeks", [0, 0, 139], [0, 0, -140]],
  ["days", [0, 0, 0, 973], [0, 0, 0, -974]],
  ["hours", [0, 0, 0, 973, 4], [0, 0, 0, -973, -5]],
  ["minutes", [0, 0, 0, 973, 4, 17], [0, 0, 0, -973, -4, -18]],
  ["seconds", [0, 0, 0, 973, 4, 17, 4], [0, 0, 0, -973, -4, -17, -5]],
  ["milliseconds", [0, 0, 0, 973, 4, 17, 4, 864], [0, 0, 0, -973, -4, -17, -4, -865]],
  ["microseconds", [0, 0, 0, 973, 4, 17, 4, 864, 197], [0, 0, 0, -973, -4, -17, -4, -864, -198]],
  ["nanoseconds", [0, 0, 0, 973, 4, 17, 4, 864, 197, 532], [0, 0, 0, -973, -4, -17, -4, -864, -197, -532]]
];
incrementOneFloor.forEach(([smallestUnit, expectedPositive, expectedNegative]) => {
  const [py, pm = 0, pw = 0, pd = 0, ph = 0, pmin = 0, ps = 0, pms = 0, pµs = 0, pns = 0] = expectedPositive;
  const [ny, nm = 0, nw = 0, nd = 0, nh = 0, nmin = 0, ns = 0, nms = 0, nµs = 0, nns = 0] = expectedNegative;
  TemporalHelpers.assertDuration(
    earlier.until(later, { smallestUnit, roundingMode: "floor" }),
    py, pm, pw, pd, ph, pmin, ps, pms, pµs, pns,
    `rounds down to ${smallestUnit} (rounding mode = floor, positive case)`
  );
  TemporalHelpers.assertDuration(
    later.until(earlier, {smallestUnit, roundingMode: "floor"}),
    ny, nm, nw, nd, nh, nmin, ns, nms, nµs, nns,
    `rounds down to ${smallestUnit} (rounding mode = floor, negative case)`
  );
});
