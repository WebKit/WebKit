// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
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
assert.throws(TypeError, () => instance.monthDayFromFields(missingDay), "day should be checked before year and month");

let getMonthCode = false;
let getYear = false;
const monthWithoutYear = {
  day: 1,
  month: 5,
  get monthCode() {
    getMonthCode = true;
  },
  get year() {
    getYear = true;
  },
};
assert.throws(TypeError, () => instance.monthDayFromFields(monthWithoutYear), "year/month should be checked after fetching but before resolving the month code");
assert(getMonthCode, "year/month is checked after fetching monthCode");
assert(getYear, "year/month is fetched after fetching month");

assert.throws(TypeError, () => instance.monthDayFromFields({ day: 1 }), "month should be resolved last");
