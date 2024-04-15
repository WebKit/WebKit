// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Errors due to missing properties on fields object are thrown in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const missingDay = {
  get year() {
    TemporalHelpers.assertUnreachable("day should be checked first");
  },
  get month() {
    TemporalHelpers.assertUnreachable("day should be checked first");
  },
  get monthCode() {
    TemporalHelpers.assertUnreachable("day should be checked first");
  },
};
assert.throws(TypeError, () => instance.monthDayFromFields(missingDay), "day should be checked before year and month");

let got = [];
const fieldsSpy = TemporalHelpers.propertyBagObserver(got, { day: 1 });
assert.throws(TypeError, () => instance.monthDayFromFields(fieldsSpy), "incomplete fields should be rejected (but after reading all non-required fields)");
assert.compareArray(got, [
  "get day",
  "get day.valueOf",
  "call day.valueOf",
  "get month",
  "get monthCode",
  "get year",
], "fields should be read in alphabetical order");
