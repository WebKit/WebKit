// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.getisofields
description: Correct field names on the object returned from getISOFields
features: [Temporal]
---*/

const md = new Temporal.PlainMonthDay(5, 2);

const result = md.getISOFields();
assert.sameValue(result.isoYear, 1972, "isoYear result");
assert.sameValue(result.isoMonth, 5, "isoMonth result");
assert.sameValue(result.isoDay, 2, "isoDay result");
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
