// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: properties around DST
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("America/Los_Angeles");
var hourBeforeDstStart = new Temporal.PlainDateTime(2020, 3, 8, 1).toZonedDateTime(tz);
var dayBeforeDstStart = new Temporal.PlainDateTime(2020, 3, 7, 2, 30).toZonedDateTime(tz);

// hoursInDay works with DST start
assert.sameValue(hourBeforeDstStart.hoursInDay, 23);

// hoursInDay works with non-DST days
assert.sameValue(dayBeforeDstStart.hoursInDay, 24);

// hoursInDay works with DST end
var dstEnd = Temporal.ZonedDateTime.from("2020-11-01T01:00-08:00[America/Los_Angeles]");
assert.sameValue(dstEnd.hoursInDay, 25);

// hoursInDay works with non-hour DST change
var zdt1 = Temporal.ZonedDateTime.from("2020-10-04T12:00[Australia/Lord_Howe]");
assert.sameValue(zdt1.hoursInDay, 23.5);
var zdt2 = Temporal.ZonedDateTime.from("2020-04-05T12:00[Australia/Lord_Howe]");
assert.sameValue(zdt2.hoursInDay, 24.5);

// hoursInDay works with non-half-hour DST change
var zdt = Temporal.ZonedDateTime.from("1933-01-01T12:00[Asia/Singapore]");
assert(Math.abs(zdt.hoursInDay - 23.666666666666668) < Number.EPSILON);

// hoursInDay works when day starts at 1:00 due to DST start at midnight
var zdt = Temporal.ZonedDateTime.from("2015-10-18T12:00:00-02:00[America/Sao_Paulo]");
assert.sameValue(zdt.hoursInDay, 23);

// startOfDay works
var start = dayBeforeDstStart.startOfDay();
assert.sameValue(`${ start.toPlainDate() }`, `${ dayBeforeDstStart.toPlainDate() }`);
assert.sameValue(`${ start.toPlainTime() }`, "00:00:00");

// startOfDay works when day starts at 1:00 due to DST start at midnight
var zdt = Temporal.ZonedDateTime.from("2015-10-18T12:00:00-02:00[America/Sao_Paulo]");
assert.sameValue(`${ zdt.startOfDay().toPlainTime() }`, "01:00:00");
var dayAfterSamoaDateLineChange = Temporal.ZonedDateTime.from("2011-12-31T22:00+14:00[Pacific/Apia]");
var dayBeforeSamoaDateLineChange = Temporal.ZonedDateTime.from("2011-12-29T22:00-10:00[Pacific/Apia]");

// startOfDay works after Samoa date line change
var start = dayAfterSamoaDateLineChange.startOfDay();
assert.sameValue(`${ start.toPlainTime() }`, "00:00:00");

// hoursInDay works after Samoa date line change
assert.sameValue(dayAfterSamoaDateLineChange.hoursInDay, 24);

// hoursInDay works before Samoa date line change
assert.sameValue(dayBeforeSamoaDateLineChange.hoursInDay, 24);
