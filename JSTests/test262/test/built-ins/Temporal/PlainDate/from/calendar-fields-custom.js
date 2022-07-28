// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: Verify the result of calendar.fields() is treated correctly.
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  dateFromFields(fields) {
    assert.compareArray(Object.keys(fields), ["b", "a"]);
    return new Temporal.PlainDate(2020, 7, 4);
  }
  fields(fields) {
    assert.compareArray(fields, ["day", "month", "monthCode", "year"]);
    return ["b", "a"];
  }
}

const calendar = new CustomCalendar();
const actual = [];
const item = new Proxy({ calendar }, {
  has(target, property) {
    actual.push(`has item.${property}`);
    return property in target;
  },
  get(target, property) {
    actual.push(`get item.${property}`);
    return target[property];
  },
});

const plainDate = Temporal.PlainDate.from(item);
TemporalHelpers.assertPlainDate(plainDate, 2020, 7, "M07", 4);
assert.compareArray(actual, [
  "get item.calendar",
  "get item.b",
  "get item.a",
]);
