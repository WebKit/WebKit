// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.tostring
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
const date = new Temporal.PlainDate(2000, 5, 2, customCalendar);
[
  ["always", "2000-05-02[u-ca=custom]", 1],
  ["auto", "2000-05-02[u-ca=custom]", 1],
  ["critical", "2000-05-02[!u-ca=custom]", 1],
  ["never", "2000-05-02", 0],
  [undefined, "2000-05-02[u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = date.toString({ calendarName });
  assert.sameValue(result, expectedResult, `toString output for calendarName = ${calendarName}`);
  assert.sameValue(calls, expectedCalls, `calls to toString for calendarName = ${calendarName}`);
});
