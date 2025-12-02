// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.datetimeformat.prototype.formattoparts
description: >
  Verifies that DateTimeFormat formats era information, with distinct values for
  calendars with multiple eras
locale: [en]
features: [Intl.Era-monthcode]
---*/

const multipleEraTests = [
  ["gregory", [-100, 2025]],
  ["japanese", [-100, 1850, 1880, 1920, 1930, 1990, 2025]],
  ["roc", [1900, 2025]],
];

for (const [calendar, isoYears] of multipleEraTests) {
  const formatter = new Intl.DateTimeFormat("en", {
    calendar,
    era: "long",
    year: "numeric",
  });

  const eras = [];

  for (const isoYear of isoYears) {
    const date = new Date(isoYear, 5, 15);
    const parts = formatter.formatToParts(date);

    const eraPart = parts.find(({ type }) => type === "era");
    assert.notSameValue(eraPart, undefined, `Format of ${calendar} ISO ${isoYear} should have era part`);
    assert.sameValue(typeof eraPart.value, "string", `Era format of ${calendar} ISO ${isoYear} should be a string`);
    assert(eraPart.value.length > 0, `Era format of ${calendar} ISO ${isoYear} should not be empty`);
    eras.push(eraPart.value);

    const format = formatter.format(date);
    assert(format.includes(eraPart.value), `${format} should include ${eraPart.value} era`);

    const yearPart = parts.find(({ type }) => type === "year");
    assert.notSameValue(yearPart, undefined, `Format of ${calendar} ISO ${isoYear} should have year part`);
  }

  assert.sameValue(new Set(eras).size, eras.length, `${calendar} eras (${eras.join(",")}) should be unique`);
}

// Calendars with a single era: test one year occurring before and one after the
// single era's epoch
const singleEraTests = [
  ["buddhist", [-600, 2025]],
  ["ethioaa", [-5550, 2025]],
  ["hebrew", [-3800, 2025]],
  ["indian", [70, 2025]],
  ["persian", [600, 2025]],
];

for (const [calendar, isoYears] of singleEraTests) {
  const formatter = new Intl.DateTimeFormat("en", {
    calendar,
    era: "long",
    year: "numeric",
  });

  const eras = [];

  for (const isoYear of isoYears) {
    const date = new Date(isoYear, 5, 15);
    const parts = formatter.formatToParts(date);

    const eraPart = parts.find(({ type }) => type === "era");
    assert.notSameValue(eraPart, undefined, `Format of ${calendar} ISO ${isoYear} should have era part`);
    assert.sameValue(typeof eraPart.value, "string", `Era format of ${calendar} ISO ${isoYear} should be a string`);
    assert(eraPart.value.length > 0, `Era format of ${calendar} ISO ${isoYear} should not be empty`);
    eras.push(eraPart.value);

    const format = formatter.format(date);
    assert(format.includes(eraPart.value), `${format} should include ${eraPart.value} era`);

    const yearPart = parts.find(({ type }) => type === "year");
    assert.notSameValue(yearPart, undefined, `Format of ${calendar} ISO ${isoYear} should have year part`);
  }

  assert.sameValue(eras[0], eras[1], `${calendar} era does not change between negative to positive years`);
}

const noEraTests = [
  "chinese",
  "dangi",
];

for (const calendar of noEraTests) {
  const formatter = new Intl.DateTimeFormat("en", {
    calendar,
    era: "long",
    year: "numeric",
  });

  const date = new Date(2025, 5, 15);
  const parts = formatter.formatToParts(date);

  const eraPart = parts.find(({ type }) => type === "era");
  assert.sameValue(eraPart, undefined, `Format of ${calendar} should not have era part`);
}
