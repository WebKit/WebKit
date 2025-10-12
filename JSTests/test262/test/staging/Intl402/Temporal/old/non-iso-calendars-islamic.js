// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Non-ISO Calendars
features: [Temporal]
locale:
  - en-US-u-ca-islamic
---*/

const calendar = "islamic";

// verify that Intl.DateTimeFormat.formatToParts output matches snapshot data
function compareFormatToPartsSnapshot(isoString, expectedComponents) {
  const date = new Date(isoString);
  const formatter = new Intl.DateTimeFormat(`en-US-u-ca-${calendar}`, { timeZone: "UTC" });
  const actualComponents = formatter.formatToParts(date);
  for (let [expectedType, expectedValue] of Object.entries(expectedComponents)) {
    const part = actualComponents.find(({type}) => type === expectedType);
    const contextMessage = `${expectedType} component of ${isoString} formatted in ${calendar}`;
    assert.notSameValue(part, undefined, contextMessage);
    assert.sameValue(part.value, `${expectedValue}`, contextMessage);
  }
}

compareFormatToPartsSnapshot("2000-01-01T00:00Z", {
  year: 1420,
  era: "AH",
  month: 9,
  // day: 25,
});

compareFormatToPartsSnapshot("0001-01-01T00:00Z", {
  year: -640,
  era: "AH",
  month: 5,
  // day: 20,
});

var fromWithCases = {
  year2000: {
    year: 1420,
    eraYear: 1420,
    era: "ah",
    month: 9,
    monthCode: "M09",
    day: [23, 25],
  },
  year1: {
    year: -640,
    eraYear: 641,
    era: "bh",
    month: 5,
    monthCode: "M05",
    day: [20, 19],
  }
};

// No Temporal tests; this calendar is not supported in Temporal
