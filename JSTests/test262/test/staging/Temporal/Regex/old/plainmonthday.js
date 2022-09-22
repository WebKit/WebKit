// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-plainmonthday-objects
description: Temporal.PlainMonthDay works as expected
features: [Temporal]
---*/

function test(isoString, components) {
  var [m, d, cid = "iso8601"] = components;
  var monthDay = Temporal.PlainMonthDay.from(isoString);
  assert.sameValue(monthDay.monthCode, `M${ m.toString().padStart(2, "0") }`);
  assert.sameValue(monthDay.day, d);
  assert.sameValue(monthDay.calendar.id, cid);
}
function generateTest(dateTimeString, zoneString) {
  var components = [
    11,
    18
  ];
  test(`${ dateTimeString }${ zoneString }`, components);
  test(`${ dateTimeString }:30${ zoneString }`, components);
  test(`${ dateTimeString }:30.123456789${ zoneString }`, components);
}
[
  "+0100[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]",
  "-0400",
  "-04:00:00.000000000",
  "+010000.0[Europe/Vienna]",
  "+01:00[+01:00]",
  "+01:00[+0100]",
  ""
].forEach(zoneString => generateTest("1976-11-18T15:23", zoneString));
[
  "1",
  "12",
  "123",
  "1234",
  "12345",
  "123456",
  "1234567",
  "12345678"
].forEach(decimals => test(`1976-11-18T15:23:30.${ decimals }`, [
  11,
  18
]));
[
  "1976-11-18T15:23:30,1234",
  "\u2212009999-11-18",
  "19761118",
  "+199999-11-18",
  "+1999991118",
  "-000300-11-18",
  "-0003001118",
  "15121118"
].forEach(str => test(str, [
  11,
  18
]));
[
  "",
  "+01:00[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]"
].forEach(zoneString => test(`1976-11-18T15:23:30.123456789${ zoneString }[u-ca=iso8601]`, [
  11,
  18
]));
test("1972-11-18[u-ca=iso8601]", [
  11,
  18
]);
