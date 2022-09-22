// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Temporal.Instant works as expected
features: [Temporal]
---*/

function test(isoString, components) {
  var [y, mon, d, h = 0, min = 0, s = 0, ms = 0, µs = 0, ns = 0] = components;
  var instant = Temporal.Instant.from(isoString);
  var utc = Temporal.TimeZone.from("UTC");
  var datetime = utc.getPlainDateTimeFor(instant);
  assert.sameValue(datetime.year, y);
  assert.sameValue(datetime.month, mon);
  assert.sameValue(datetime.day, d);
  assert.sameValue(datetime.hour, h);
  assert.sameValue(datetime.minute, min);
  assert.sameValue(datetime.second, s);
  assert.sameValue(datetime.millisecond, ms);
  assert.sameValue(datetime.microsecond, µs);
  assert.sameValue(datetime.nanosecond, ns);
}
function generateTest(dateTimeString, zoneString, components) {
  test(`${ dateTimeString }${ zoneString }`, components.slice(0, 5));
  test(`${ dateTimeString }:30${ zoneString }`, components.slice(0, 6));
  test(`${ dateTimeString }:30.123456789${ zoneString }`, components);
}
// valid strings
test("2020-01-01Z", [
  2020,
  1,
  1,
  0,
  0,
  0
]);
[
  "+01:00",
  "+01",
  "+0100",
  "+01:00:00",
  "+010000",
  "+01:00:00.000000000",
  "+010000.0"
].forEach(zoneString => {
  generateTest("1976-11-18T15:23", `${ zoneString }[Europe/Vienna]`, [
    1976,
    11,
    18,
    14,
    23,
    30,
    123,
    456,
    789
  ]);
  generateTest("1976-11-18T15:23", `+01:00[${ zoneString }]`, [
    1976,
    11,
    18,
    14,
    23,
    30,
    123,
    456,
    789
  ]);
});
[
  "-04:00",
  "-04",
  "-0400",
  "-04:00:00",
  "-040000",
  "-04:00:00.000000000",
  "-040000.0"
].forEach(zoneString => generateTest("1976-11-18T15:23", zoneString, [
  1976,
  11,
  18,
  19,
  23,
  30,
  123,
  456,
  789
]));
test("1976-11-18T15:23:30.1Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  100
]);
test("1976-11-18T15:23:30.12Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  120
]);
test("1976-11-18T15:23:30.123Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123
]);
test("1976-11-18T15:23:30.1234Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  400
]);
test("1976-11-18T15:23:30.12345Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  450
]);
test("1976-11-18T15:23:30.123456Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  456
]);
test("1976-11-18T15:23:30.1234567Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  456,
  700
]);
test("1976-11-18T15:23:30.12345678Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  456,
  780
]);
generateTest("1976-11-18T15:23", "z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  456,
  789
]);
test("1976-11-18T15:23:30,1234Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  400
]);
[
  "\u221204:00",
  "\u221204",
  "\u22120400"
].forEach(offset => test(`1976-11-18T15:23:30.1234${ offset }`, [
  1976,
  11,
  18,
  19,
  23,
  30,
  123,
  400
]));
test("\u2212009999-11-18T15:23:30.1234Z", [
  -9999,
  11,
  18,
  15,
  23,
  30,
  123,
  400
]);
test("1976-11-18T152330Z", [
  1976,
  11,
  18,
  15,
  23,
  30
]);
test("1976-11-18T152330.1234Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  400
]);
test("19761118T15:23:30Z", [
  1976,
  11,
  18,
  15,
  23,
  30
]);
test("19761118T152330Z", [
  1976,
  11,
  18,
  15,
  23,
  30
]);
test("19761118T152330.1234Z", [
  1976,
  11,
  18,
  15,
  23,
  30,
  123,
  400
]);
test("1976-11-18T15Z", [
  1976,
  11,
  18,
  15
]);

