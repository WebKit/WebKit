// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.tostring
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
const date = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", customCalendar);
[
  ["always", "2001-09-09T01:46:40.987654321+00:00[UTC][u-ca=custom]", 1],
  ["auto", "2001-09-09T01:46:40.987654321+00:00[UTC][u-ca=custom]", 1],
  ["critical", "2001-09-09T01:46:40.987654321+00:00[UTC][!u-ca=custom]", 1],
  ["never", "2001-09-09T01:46:40.987654321+00:00[UTC]", 0],
  [undefined, "2001-09-09T01:46:40.987654321+00:00[UTC][u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = date.toString({ calendarName });
  assert.sameValue(result, expectedResult, `toString output for calendarName = ${calendarName}`);
  assert.sameValue(calls, expectedCalls, `calls to toString for calendarName = ${calendarName}`);
});
