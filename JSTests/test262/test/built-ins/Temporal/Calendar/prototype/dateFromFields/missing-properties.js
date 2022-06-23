// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Errors due to missing properties on fields object are thrown in the correct order
includes: [temporalHelpers.js]
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
assert.throws(TypeError, () => instance.dateFromFields(missingDay), "day should be checked before year and month");

let getMonth = false;
let getMonthCode = false;
const missingYearAndMonth = {
  day: 1,
  get month() {
    getMonth = true;
  },
  get monthCode() {
    getMonthCode = true;
  },
};
assert.throws(TypeError, () => instance.dateFromFields(missingYearAndMonth), "year should be checked after fetching but before resolving the month");
assert(getMonth, "year is fetched after month");
assert(getMonthCode, "year is fetched after monthCode");

const missingMonth = {
  day: 1,
  year: 2000,
};
assert.throws(TypeError, () => instance.dateFromFields(missingMonth), "month should be resolved last");
