// Copyright 2019 Igalia, S.L., Google, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.numberformat.prototype.format
description: Checks handling of units.
features: [Intl.NumberFormat-unified]
---*/

function check(unit) {
  const s1 = (123).toLocaleString(undefined, { style: "unit", unit: unit });
  const s2 = (123).toLocaleString();
  assert.notSameValue(s1, s2);
}

const units = [
  "acre",
  "bit",
  "byte",
  "celsius",
  "centimeter",
  "day",
  "degree",
  "fahrenheit",
  "fluid-ounce",
  "foot",
  "gallon",
  "gigabit",
  "gigabyte",
  "gram",
  "hectare",
  "hour",
  "inch",
  "kilobit",
  "kilobyte",
  "kilogram",
  "kilometer",
  "liter",
  "megabit",
  "megabyte",
  "meter",
  "mile",
  "mile-scandinavian",
  "millimeter",
  "milliliter",
  "millisecond",
  "minute",
  "month",
  "ounce",
  "percent",
  "petabyte",
  "pound",
  "second",
  "stone",
  "terabit",
  "terabyte",
  "week",
  "yard",
  "year",
];

for (const simpleUnit of units) {
  check(simpleUnit);
  for (const simpleUnit2 of units) {
    check(simpleUnit + "-per-" + simpleUnit2);
    check(simpleUnit2 + "-per-" + simpleUnit);
  }
}
