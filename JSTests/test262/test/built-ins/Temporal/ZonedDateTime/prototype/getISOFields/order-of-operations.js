// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
description: >
  Properties on the calendar or time zone of the receiver of getISOFields()
  are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
];
const actual = [];

const instance = new Temporal.ZonedDateTime(
  988786472_987_654_321n,  /* 2001-05-02T06:54:32.987654321Z */
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone"),
  TemporalHelpers.calendarObserver(actual, "this.calendar"),
);
// clear any observable operations that happen due to time zone or calendar
// calls on the constructor
actual.splice(0);

instance.getISOFields();
assert.compareArray(actual, expected, "order of operations");
