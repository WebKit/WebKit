// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tolocalestring
description: Basic tests that dateStyle option affects output
locale: [en-u-ca-gregory, en-u-ca-islamic]
features: [Temporal, Intl.DateTimeFormat-datetimestyle]
---*/

const dateGregorian = new Temporal.PlainDate(2024, 3, 26, "gregory");

assert(
  dateGregorian.toLocaleString("en-u-ca-gregory", { dateStyle: "long" }).includes("March"),
  "dateStyle: long writes month of March out in full"
);
assert(
  !dateGregorian.toLocaleString("en-u-ca-gregory", { dateStyle: "short" }).includes("March"),
  "dateStyle: short does not write month of March out in full"
);

const dateIslamic = new Temporal.PlainDate(2024, 3, 26, "islamic");

assert(
  dateIslamic.toLocaleString("en-u-ca-islamic", { dateStyle: "long" }).includes("Ramadan"),
  "dateStyle: long writes month of Ramadan out in full"
);
assert(
  !dateIslamic.toLocaleString("en-u-ca-islamic", { dateStyle: "short" }).includes("Ramadan"),
  "dateStyle: short does not write month of Ramadan out in full"
);
