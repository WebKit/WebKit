// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.tostring
description: Should always call 'toString' on the calendar once.
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
  ["always", "2000-05-02[u-ca=custom]"],
  ["auto", "2000-05-02[u-ca=custom]"],
  ["never", "2000-05-02"],
  [undefined, "2000-05-02[u-ca=custom]"],
].forEach(([calendarName, expected]) => {
  calls = 0;
  const result = date.toString({ calendarName });
  assert.sameValue(result, expected, `calendarName = ${calendarName}: expected ${expected}`);
  assert.sameValue(calls, 1, `calendarName = ${calendarName}: expected one call to 'toString'`);
});
