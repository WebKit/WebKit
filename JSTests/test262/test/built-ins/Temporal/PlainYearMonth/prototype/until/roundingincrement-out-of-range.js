// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: RangeError thrown when roundingIncrement option out of range
info: |
    sec-temporal-totemporalroundingincrement step 6:
      6. If _increment_ < 1 or _increment_ > _maximum_, throw a *RangeError* exception.
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2000, 10);
assert.throws(RangeError, () => earlier.until(later, { roundingIncrement: -Infinity }));
assert.throws(RangeError, () => earlier.until(later, { roundingIncrement: -1 }));
assert.throws(RangeError, () => earlier.until(later, { roundingIncrement: 0 }));
assert.throws(RangeError, () => earlier.until(later, { roundingIncrement: Infinity }));
