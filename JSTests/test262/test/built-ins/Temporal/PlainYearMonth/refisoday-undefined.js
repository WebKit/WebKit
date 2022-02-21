// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth
description: referenceISODay argument defaults to 1 if not given
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const args = [2000, 5, calendar];

const dateExplicit = new Temporal.PlainYearMonth(...args, undefined);
assert.sameValue(dateExplicit.getISOFields().isoDay, 1, "default referenceISODay is 1");

const dateImplicit = new Temporal.PlainYearMonth(...args);
assert.sameValue(dateImplicit.getISOFields().isoDay, 1, "default referenceISODay is 1");
