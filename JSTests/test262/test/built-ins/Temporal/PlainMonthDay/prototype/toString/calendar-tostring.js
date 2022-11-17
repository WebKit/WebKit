// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.protoype.tostring
description: Number of observable 'toString' calls on the calendar for each value of calendarName
features: [Temporal]
---*/

let calls;
const customCalendar = {
  toString() {
    ++calls;
    return "custom";
  }
};
const monthday = new Temporal.PlainMonthDay(5, 2, customCalendar);
[
  ["always", "1972-05-02[u-ca=custom]", 1],
  ["auto", "1972-05-02[u-ca=custom]", 1],
  ["critical", "1972-05-02[!u-ca=custom]", 1],
  ["never", "1972-05-02", 1],
  [undefined, "1972-05-02[u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = monthday.toString({ calendarName });
  assert.sameValue(result, expectedResult, `toString output for calendarName = ${calendarName}`);
  assert.sameValue(calls, expectedCalls, `calls to toString for calendarName = ${calendarName}`);
});
