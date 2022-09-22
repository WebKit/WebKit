// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.tostring
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
const date = new Temporal.ZonedDateTime(3661_987_654_321n, "UTC", customCalendar);
[
  ["always", "1970-01-01T01:01:01.987654321+00:00[UTC][u-ca=custom]", 1],
  ["auto", "1970-01-01T01:01:01.987654321+00:00[UTC][u-ca=custom]", 1],
  ["never", "1970-01-01T01:01:01.987654321+00:00[UTC]", 0],
  [undefined, "1970-01-01T01:01:01.987654321+00:00[UTC][u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = date.toString({ calendarName });
  assert.sameValue(result, expectedResult, `calendarName = ${calendarName}: expected ${expectedResult}`);
  assert.sameValue(calls, expectedCalls, `calendarName = ${calendarName}: expected ${expectedCalls} call(s) to 'toString'`);
});
