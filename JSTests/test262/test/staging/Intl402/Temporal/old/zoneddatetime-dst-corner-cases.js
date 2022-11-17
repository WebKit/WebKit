// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Corner cases of time zone offset shifts
features: [Temporal]
---*/

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

// startOfDay works when day starts at 1:00 due to DST start at midnight
var zdt = Temporal.ZonedDateTime.from("2015-10-18T12:00:00-02:00[America/Sao_Paulo]");
assert.sameValue(`${ zdt.startOfDay().toPlainTime() }`, "01:00:00");
