// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-plaindatetime-objects
description: Temporal.PlainDateTime works as expected
features: [Temporal]
---*/

function test(isoString, components) {
  var [y, mon, d, h = 0, min = 0, s = 0, ms = 0, µs = 0, ns = 0, cid = "iso8601"] = components;
  var datetime = Temporal.PlainDateTime.from(isoString);
  assert.sameValue(datetime.year, y);
  assert.sameValue(datetime.month, mon);
  assert.sameValue(datetime.day, d);
  assert.sameValue(datetime.hour, h);
  assert.sameValue(datetime.minute, min);
  assert.sameValue(datetime.second, s);
  assert.sameValue(datetime.millisecond, ms);
  assert.sameValue(datetime.microsecond, µs);
  assert.sameValue(datetime.nanosecond, ns);
  assert.sameValue(datetime.calendar.id, cid);
}
function generateTest(dateTimeString, zoneString) {
  var components = [
    1976,
    11,
    18,
    15,
    23,
    30,
    123,
    456,
    789
  ];
  test(`${ dateTimeString }${ zoneString }`, components.slice(0, 5));
  test(`${ dateTimeString }:30${ zoneString }`, components.slice(0, 6));
  test(`${ dateTimeString }:30.123456789${ zoneString }`, components);
}

//valid strings 
[
  "+0100[Europe/Vienna]",
  "+01:00[Europe/Vienna]",
  "[Europe/Vienna]",
  "+01:00[Custom/Vienna]",
  "-0400",
  "-04:00",
  "-04:00:00.000000000",
  "+010000.0[Europe/Vienna]",
  "+01:00[+01:00]",
  "+01:00[+0100]",
  ""
].forEach(zoneString => generateTest("1976-11-18T15:23", zoneString));
[
  "",
  "+01:00[Europe/Vienna]",
  "+01:00[Custom/Vienna]",
  "[Europe/Vienna]"
].forEach(zoneString => test(`1976-11-18T15:23:30.123456789${ zoneString }[u-ca=iso8601]`, [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  456,
  789
]));

