// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.protoype.tostring
description: Should call 'toString' on the calendar once unless calendarName == 'never'.
features: [Temporal]
---*/

let calls;
const customCalendar = {
  toString() {
    ++calls;
    return "custom";
  }
};
const date = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, customCalendar);
[
  ["always", "2000-05-02T12:34:56.987654321[u-ca=custom]", 1],
  ["auto", "2000-05-02T12:34:56.987654321[u-ca=custom]", 1],
  ["never", "2000-05-02T12:34:56.987654321", 0],
  [undefined, "2000-05-02T12:34:56.987654321[u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = date.toString({ calendarName });
  assert.sameValue(result, expectedResult, `calendarName = ${calendarName}: expected ${expectedResult}`);
  assert.sameValue(calls, expectedCalls, `calendarName = ${calendarName}: expected ${expectedCalls} call(s) to 'toString'`);
});
