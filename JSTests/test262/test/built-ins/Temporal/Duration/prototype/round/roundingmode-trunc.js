// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: Tests calculations with roundingMode "trunc".
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Duration(5, 6, 7, 8, 40, 30, 20, 123, 987, 500);
const relativeTo = new Temporal.PlainDate(2020, 1, 1);

const expected = [
  ["years", [5], [-5]],
  ["months", [5, 7], [-5, -7]],
  ["weeks", [5, 6, 8], [-5, -6, -8]],
  ["days", [5, 6, 7, 9], [-5, -6, -7, -9]],
  ["hours", [5, 6, 7, 9, 16], [-5, -6, -7, -9, -16]],
  ["minutes", [5, 6, 7, 9, 16, 30], [-5, -6, -7, -9, -16, -30]],
  ["seconds", [5, 6, 7, 9, 16, 30, 20], [-5, -6, -7, -9, -16, -30, -20]],
  ["milliseconds", [5, 6, 7, 9, 16, 30, 20, 123], [-5, -6, -7, -9, -16, -30, -20, -123]],
  ["microseconds", [5, 6, 7, 9, 16, 30, 20, 123, 987], [-5, -6, -7, -9, -16, -30, -20, -123, -987]],
  ["nanoseconds", [5, 6, 7, 9, 16, 30, 20, 123, 987, 500], [-5, -6, -7, -9, -16, -30, -20, -123, -987, -500]],
];

const roundingMode = "trunc";

expected.forEach(([smallestUnit, expectedPositive, expectedNegative]) => {
  const [py, pm = 0, pw = 0, pd = 0, ph = 0, pmin = 0, ps = 0, pms = 0, pµs = 0, pns = 0] = expectedPositive;
  const [ny, nm = 0, nw = 0, nd = 0, nh = 0, nmin = 0, ns = 0, nms = 0, nµs = 0, nns = 0] = expectedNegative;
  TemporalHelpers.assertDuration(
    instance.round({ smallestUnit, relativeTo, roundingMode }),
    py, pm, pw, pd, ph, pmin, ps, pms, pµs, pns,
    `rounds to ${smallestUnit} (roundingMode = ${roundingMode}, positive case)`
  );
  TemporalHelpers.assertDuration(
    instance.negated().round({ smallestUnit, relativeTo, roundingMode }),
    ny, nm, nw, nd, nh, nmin, ns, nms, nµs, nns,
    `rounds to ${smallestUnit} (rounding mode = ${roundingMode}, negative case)`
  );
});
