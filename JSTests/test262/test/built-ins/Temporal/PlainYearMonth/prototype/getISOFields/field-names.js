// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.getisofields
description: Correct field names on the object returned from getISOFields
features: [Temporal]
---*/

const ym = new Temporal.PlainYearMonth(2000, 5);

const result = ym.getISOFields();
assert.sameValue(result.isoYear, 2000, "isoYear result");
assert.sameValue(result.isoMonth, 5, "isoMonth result");
assert.sameValue(result.isoDay, 1, "isoDay result");
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
