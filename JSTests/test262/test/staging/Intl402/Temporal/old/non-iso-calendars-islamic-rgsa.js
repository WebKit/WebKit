// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Non-ISO Calendars
features: [Temporal]
locale:
  - en-US-u-ca-islamic-rgsa
---*/

const calendar = "islamic-rgsa";

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

// No Temporal tests; this calendar is not supported in Temporal
