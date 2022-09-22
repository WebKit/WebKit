// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-plainyearmonth-objects
description: Temporal.PlainYearMonth works as expected
features: [Temporal]
---*/

function test(isoString, components) {
  var [y, m, cid = "iso8601"] = components;
  var yearMonth = Temporal.PlainYearMonth.from(isoString);
  assert.sameValue(yearMonth.year, y);
  assert.sameValue(yearMonth.month, m);
  assert.sameValue(yearMonth.calendar.id, cid);
}
function generateTest(dateTimeString, zoneString) {
  var components = [
    1976,
    11
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
  "+01:00:00.0[Europe/Vienna]",
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
  1976,
  11
]));
test("1976-11-18T15:23:30,1234", [
  1976,
  11
]);
test("19761118", [
  1976,
  11
]);
test("+199999-11-18", [
  199999,
  11
]);
test("+1999991118", [
  199999,
  11
]);
test("-000300-11-18", [
  -300,
  11
]);
test("-0003001118", [
  -300,
  11
]);
test("1512-11-18", [
  1512,
  11
]);
test("+199999-11", [
  199999,
  11
]);
test("+19999911", [
  199999,
  11
]);
test("-000300-11", [
  -300,
  11
]);
test("-00030011", [
  -300,
  11
]);
[
  "",
  "+01:00[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]"
].forEach(zoneString => test(`1976-11-18T15:23:30.123456789${ zoneString }[u-ca=iso8601]`, [
  1976,
  11
]));
test("1976-11-01[u-ca=iso8601]", [
  1976,
  11
]);
