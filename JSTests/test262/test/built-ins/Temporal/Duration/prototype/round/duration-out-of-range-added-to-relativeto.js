// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: RangeError thrown when calendar part of duration added to relativeTo is out of range
features: [Temporal]
info: |
  RoundDuration:
  8.k. Let _isoResult_ be ! AddISODate(_plainRelativeTo_.[[ISOYear]]. _plainRelativeTo_.[[ISOMonth]], _plainRelativeTo_.[[ISODay]], 0, 0, 0, truncate(_fractionalDays_), *"constrain"*).
    l. Let _wholeDaysLater_ be ? CreateTemporalDate(_isoResult_.[[Year]], _isoResult_.[[Month]], _isoResult_.[[Day]], _calendar_).
---*/

// Based on a test case by André Bargull <andre.bargull@gmail.com>

const instance = new Temporal.Duration(0, 0, 0, /* days = */ 500_000_000);
const relativeTo = new Temporal.PlainDate(2000, 1, 1);
assert.throws(RangeError, () => instance.round({relativeTo, smallestUnit: "years"}));

const negInstance = new Temporal.Duration(0, 0, 0, /* days = */ -500_000_000);
assert.throws(RangeError, () => negInstance.round({relativeTo, smallestUnit: "years"}));
