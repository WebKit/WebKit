// Copyright 2019 Google Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
  Checks the order of getting "calendar" and "numberingSystem" options in the
  DateTimeFormat is between "localeMatcher" and "hour12" options.
info: |
  4. Let _matcher_ be ? GetOption(_options_, `"localeMatcher"`, `"string"`, &laquo; `"lookup"`, `"best fit"` &raquo;, `"best fit"`).
  ...
  6. Let _calendar_ be ? GetOption(_options_, `"calendar"`, `"string"`, *undefined*, *undefined*).
  ...
  9. Let _numberingSystem_ be ? GetOption(_options_, `"numberingSystem"`, `"string"`, *undefined*, *undefined*).
  ...
  12. Let _hour12_ be ? GetOption(_options_, `"hour12"`, `"boolean"`, *undefined*, *undefined*).
includes: [compareArray.js]
---*/

const actual = [];

const options = {
  get localeMatcher() {
    actual.push("localeMatcher");
    return undefined;
  },
  get calendar() {
    actual.push("calendar");
    return undefined;
  },
  get numberingSystem() {
    actual.push("numberingSystem");
    return undefined;
  },
  get hour12() {
    actual.push("hour12");
    return undefined;
  },
};

const expected = [
  "localeMatcher",
  "calendar",
  "numberingSystem",
  "hour12"
];

let df = new Intl.DateTimeFormat(undefined, options);
assert.compareArray(actual, expected);
