// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday
description: referenceISOYear argument defaults to 1972 if not given
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const args = [5, 2, calendar];

const dateExplicit = new Temporal.PlainMonthDay(...args, undefined);
assert.sameValue(dateExplicit.getISOFields().isoYear, 1972, "default referenceISOYear is 1972");

const dateImplicit = new Temporal.PlainMonthDay(...args);
assert.sameValue(dateImplicit.getISOFields().isoYear, 1972, "default referenceISOYear is 1972");
