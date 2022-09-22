// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-plainedate-objects
description: Temporal.PlainDate works as expected
features: [Temporal]
---*/

function test(isoString, components) {
    var [y, m, d, cid = "iso8601"] = components;
    var date = Temporal.PlainDate.from(isoString);
    assert.sameValue(date.year, y);
    assert.sameValue(date.month, m);
    assert.sameValue(date.day, d);
    assert.sameValue(date.calendar.id, cid);
}
function generateTest(dateTimeString, zoneString) {
  var components = [
    1976,
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
  11,
  18
]));
test("1976-11-18T15:23:30,1234", [
  1976,
  11,
  18
]);
test("\u2212009999-11-18", [
  -9999,
  11,
  18
]);
test("1976-11-18T15", [
  1976,
  11,
  18
]);
[
  "",
  "+01:00[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]"
].forEach(zoneString => test(`1976-11-18T15:23:30.123456789${ zoneString }[u-ca=iso8601]`, [
  1976,
  11,
  18
]));
test("1976-11-18[u-ca=iso8601]", [
  1976,
  11,
  18
]);
