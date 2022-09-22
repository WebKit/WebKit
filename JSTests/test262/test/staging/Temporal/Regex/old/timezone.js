// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: Temporal.TimeZone works as expected
features: [Temporal]
---*/

function test(offsetString, expectedName) {
  var timeZone = Temporal.TimeZone.from(offsetString);
  assert.sameValue(timeZone.id, expectedName);
}
function generateTest(dateTimeString, zoneString, expectedName) {
  test(`${ dateTimeString }${ zoneString }`, expectedName);
  test(`${ dateTimeString }:30${ zoneString }`, expectedName);
  test(`${ dateTimeString }:30.123456789${ zoneString }`, expectedName);
}
[
  "+01:00",
  "+01",
  "+0100",
  "+01:00:00",
  "+010000",
  "+01:00:00.000000000",
  "+010000.0"
].forEach(zoneString => {
  generateTest("1976-11-18T15:23", `${ zoneString }[Europe/Vienna]`, "Europe/Vienna");
  generateTest("1976-11-18T15:23", `+01:00[${ zoneString }]`, "+01:00");
});
[
  "-04:00",
  "-04",
  "-0400",
  "-04:00:00",
  "-040000",
  "-04:00:00.000000000",
  "-040000.0"
].forEach(zoneString => generateTest("1976-11-18T15:23", zoneString, "-04:00"));
[
  "1",
  "12",
  "123",
  "1234",
  "12345",
  "123456",
  "1234567",
  "12345678"
].forEach(decimals => {
  test(`1976-11-18T15:23:30.${ decimals }Z`, "UTC");
  test(`1976-11-18T15:23+01:00[+01:00:00.${ decimals }]`, `+01:00:00.${ decimals }`);
});
generateTest("1976-11-18T15:23", "z", "UTC");
test("1976-11-18T15:23:30,1234Z", "UTC");
test("1976-11-18T15:23-04:00:00,000000000", "-04:00");
test("1976-11-18T15:23+010000,0[Europe/Vienna]", "Europe/Vienna");
[
  "\u221204:00",
  "\u221204",
  "\u22120400"
].forEach(offset => test(`1976-11-18T15:23${ offset }`, "-04:00"));
[
  "1976-11-18T152330",
  "1976-11-18T152330.1234",
  "19761118T15:23:30",
  "19761118T152330",
  "19761118T152330.1234",
  "1976-11-18T15"
].forEach(dateTimeString => {
  [
    "+01:00",
    "+01",
    "+0100",
    ""
  ].forEach(zoneString => test(`${ dateTimeString }${ zoneString }[Europe/Vienna]`, "Europe/Vienna"));
  [
    "-04:00",
    "-04",
    "-0400"
  ].forEach(zoneString => test(`${ dateTimeString }${ zoneString }`, "-04:00"));
  test(`${ dateTimeString }Z`, "UTC");
});
test("-0000", "+00:00");
test("-00:00", "+00:00");
test("+00", "+00:00");
test("-00", "+00:00");
test("+03", "+03:00");
test("-03", "-03:00");
test("\u22120000", "+00:00");
test("\u221200:00", "+00:00");
test("\u221200", "+00:00");
test("\u22120300", "-03:00");
test("\u221203:00", "-03:00");
test("\u221203", "-03:00");
test("+030000.0", "+03:00");
test("-03:00:00", "-03:00");
test("-03:00:00.000000000", "-03:00");
test("1976-11-18T15:23:30.123456789Z[u-ca=iso8601]", "UTC");
test("1976-11-18T15:23:30.123456789-04:00[u-ca=iso8601]", "-04:00");
test("1976-11-18T15:23:30.123456789[Europe/Vienna][u-ca=iso8601]", "Europe/Vienna");
test("1976-11-18T15:23:30.123456789+01:00[Europe/Vienna][u-ca=iso8601]", "Europe/Vienna");
