// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.compare
description: A calendar ID is valid input for Calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dateFromFieldsOriginal = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "dateFromFields");
Object.defineProperty(Temporal.Calendar.prototype, "dateFromFields", {
  configurable: true,
  enumerable: false,
  get() {
    TemporalHelpers.assertUnreachable("dateFromFields should not be looked up");
  },
});

const calendar = "iso8601";

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };

const result1 = Temporal.PlainDate.compare(arg, new Temporal.PlainDate(1976, 11, 18));
assert.sameValue(result1, 0, `Calendar created from string "${arg}" (first argument)`);

const result2 = Temporal.PlainDate.compare(new Temporal.PlainDate(1976, 11, 18), arg);
assert.sameValue(result2, 0, `Calendar created from string "${arg}" (second argument)`);

Object.defineProperty(Temporal.Calendar.prototype, "dateFromFields", dateFromFieldsOriginal);
