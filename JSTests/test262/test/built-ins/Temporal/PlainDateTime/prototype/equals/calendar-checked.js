// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.equals
description: Calendar is taken into account if the ISO data is equal
includes: [compareArray.js,temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const calendar1 = TemporalHelpers.toPrimitiveObserver(actual, "A", "calendar1");
const calendar2 = TemporalHelpers.toPrimitiveObserver(actual, "A", "calendar2");
const calendar3 = TemporalHelpers.toPrimitiveObserver(actual, "B", "calendar3");
const dt1 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar1);
const dt1b = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar1);
const dt2 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar2);
const dt3 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar3);

assert.sameValue(dt1.equals(dt1b), true, "same calendar object");
assert.compareArray(actual, []);

assert.sameValue(dt1.equals(dt2), true, "same calendar string");
assert.compareArray(actual, ["get calendar1.toString", "call calendar1.toString", "get calendar2.toString", "call calendar2.toString"]);

actual.splice(0);  // empty it for the next check
assert.sameValue(dt1.equals(dt3), false, "different calendar string");
assert.compareArray(actual, ["get calendar1.toString", "call calendar1.toString", "get calendar3.toString", "call calendar3.toString"]);

const calendar4 = { toString() { throw new Test262Error("should not call calendar4.toString") } };
const calendar5 = { toString() { throw new Test262Error("should not call calendar5.toString") } };
const dt4 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar4);
const dt5 = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102, calendar4);
const dt6 = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102, calendar5);
assert.sameValue(dt4.equals(dt5), false, "not equal same calendar");
assert.sameValue(dt4.equals(dt6), false, "not equal different calendar");
