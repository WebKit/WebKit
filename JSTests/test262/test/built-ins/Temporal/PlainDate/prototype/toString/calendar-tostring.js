// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.tostring
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
const date = new Temporal.PlainDate(2000, 5, 2, customCalendar);
[
  ["always", "2000-05-02[u-ca=custom]", 1],
  ["auto", "2000-05-02[u-ca=custom]", 1],
  ["never", "2000-05-02", 0],
  [undefined, "2000-05-02[u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = date.toString({ calendarName });
  assert.sameValue(result, expectedResult, `calendarName = ${calendarName}: expected ${expectedResult}`);
  assert.sameValue(calls, expectedCalls, `calendarName = ${calendarName}: expected ${expectedCalls} call(s) to 'toString'`);
});
