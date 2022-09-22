// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-plaintime-objects
description: Temporal.PlainTime works as expected
features: [Temporal]
---*/

function test(isoString, components) {
  var [h = 0, m = 0, s = 0, ms = 0, µs = 0, ns = 0] = components;
  var time = Temporal.PlainTime.from(isoString);
  assert.sameValue(time.hour, h);
  assert.sameValue(time.minute, m);
  assert.sameValue(time.second, s);
  assert.sameValue(time.millisecond, ms);
  assert.sameValue(time.microsecond, µs);
  assert.sameValue(time.nanosecond, ns);
}
function generateTest(dateTimeString, zoneString) {
  var components = [
    15,
    23,
    30,
    123,
    456,
    789
  ];
  test(`${ dateTimeString }${ zoneString }`, components.slice(0, 2));
  test(`${ dateTimeString }:30${ zoneString }`, components.slice(0, 3));
  test(`${ dateTimeString }:30.123456789${ zoneString }`, components);
}
[
  "+0100[Europe/Vienna]",
  "+01:00[Custom/Vienna]",
  "-0400",
  "-04:00:00.000000000",
  "+010000.0[Europe/Vienna]",
  "+01:00[+01:00]",
  "+01:00[+0100]",
  ""
].forEach(zoneString => generateTest("1976-11-18T15:23", zoneString));
[
  "+01:00[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]",
  "-04:00",
  ""
].forEach(zoneStr => test(`15${ zoneStr }`, [15]));
[
  "",
  "+01:00[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]"
].forEach(zoneString => test(`1976-11-18T15:23:30.123456789${ zoneString }[u-ca=iso8601]`, [
  15,
  23,
  30,
  123,
  456,
  789
]));
