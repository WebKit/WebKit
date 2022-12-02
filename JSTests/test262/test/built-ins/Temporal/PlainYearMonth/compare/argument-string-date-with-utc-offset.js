// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: UTC offset not valid with format that does not include a time
features: [Temporal]
---*/

const validStrings = [
  "2019-12[Africa/Abidjan]",
  "2019-12[!Africa/Abidjan]",
  "2019-12[u-ca=iso8601]",
  "2019-12[Africa/Abidjan][u-ca=iso8601]",
  "2019-12-15T00+00:00",
  "2019-12-15T00+00:00[UTC]",
  "2019-12-15T00+00:00[!UTC]",
  "2019-12-15T00-02:30[America/St_Johns]",
];

for (const arg of validStrings) {
  const result = Temporal.PlainYearMonth.compare(arg, arg);

  assert.sameValue(
    result,
    0,
    `"${arg}" is a valid UTC offset with time for PlainYearMonth`
  );
}

const invalidStrings = [
  "2022-09[u-ca=hebrew]",
  "2022-09Z",
  "2022-09+01:00",
  "2022-09-15Z",
  "2022-09-15Z[UTC]",
  "2022-09-15Z[Europe/Vienna]",
  "2022-09-15+00:00",
  "2022-09-15+00:00[UTC]",
  "2022-09-15-02:30",
  "2022-09-15-02:30[America/St_Johns]",
];

for (const arg of invalidStrings) {
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.compare(arg, new Temporal.PlainYearMonth(2019, 6)),
    `"${arg}" UTC offset without time is not valid for PlainYearMonth (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.compare(new Temporal.PlainYearMonth(2019, 6), arg),
    `"${arg}" UTC offset without time is not valid for PlainYearMonth (second argument)`
  );
}
