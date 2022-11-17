// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: properties around DST
includes: [temporalHelpers.js]
features: [Temporal]
---*/

var tz = TemporalHelpers.springForwardFallBackTimeZone();
var hourBeforeDstStart = new Temporal.PlainDateTime(2000, 4, 2, 1).toZonedDateTime(tz);
var dayBeforeDstStart = new Temporal.PlainDateTime(2000, 4, 1, 2, 30).toZonedDateTime(tz);

// hoursInDay works with DST start
assert.sameValue(hourBeforeDstStart.hoursInDay, 23);

// hoursInDay works with non-DST days
assert.sameValue(dayBeforeDstStart.hoursInDay, 24);

// hoursInDay works with DST end
var dstEnd = Temporal.PlainDateTime.from("2000-10-29T01:00").toZonedDateTime(tz);
assert.sameValue(dstEnd.hoursInDay, 25);

// startOfDay works
var start = dayBeforeDstStart.startOfDay();
assert.sameValue(`${ start.toPlainDate() }`, `${ dayBeforeDstStart.toPlainDate() }`);
assert.sameValue(`${ start.toPlainTime() }`, "00:00:00");

var samoa = TemporalHelpers.crossDateLineTimeZone();
var dayAfterSamoaDateLineChange = Temporal.PlainDateTime.from("2011-12-31T22:00").toZonedDateTime(samoa);
var dayBeforeSamoaDateLineChange = Temporal.PlainDateTime.from("2011-12-29T22:00").toZonedDateTime(samoa);

// startOfDay works after Samoa date line change
var start = dayAfterSamoaDateLineChange.startOfDay();
assert.sameValue(`${ start.toPlainTime() }`, "00:00:00");

// hoursInDay works after Samoa date line change
assert.sameValue(dayAfterSamoaDateLineChange.hoursInDay, 24);

// hoursInDay works before Samoa date line change
assert.sameValue(dayBeforeSamoaDateLineChange.hoursInDay, 24);
