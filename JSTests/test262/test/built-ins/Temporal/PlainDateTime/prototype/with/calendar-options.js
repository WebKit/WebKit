// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.with
description: The options argument is passed through to Calendar#dateFromFields as-is.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const options = {};
let calledDateFromFields = 0;
class Calendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateFromFields(fields, optionsArg) {
    ++calledDateFromFields;
    assert.sameValue(optionsArg, options, "should pass options object through");
    return super.dateFromFields(fields, optionsArg);
  }
};
const calendar = new Calendar();
const plaindatetime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
const result = plaindatetime.with({ year: 2005 }, options);
TemporalHelpers.assertPlainDateTime(result, 2005, 5, "M05", 2, 12, 34, 56, 987, 654, 321);
assert.sameValue(calledDateFromFields, 1, "should have called overridden dateFromFields once");
