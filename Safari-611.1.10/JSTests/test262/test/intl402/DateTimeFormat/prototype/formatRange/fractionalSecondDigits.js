// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: Checks basic handling of fractionalSecondDigits.
features: [Intl.DateTimeFormat-fractionalSecondDigits, Intl.DateTimeFormat-formatRange]
locale: [en-US]
---*/

const d1 = new Date(2019, 7, 10,  1, 2, 3, 234);
const d2 = new Date(2019, 7, 10,  1, 2, 3, 567);
const d3 = new Date(2019, 7, 10,  1, 2, 13, 987);

let dtf = new Intl.DateTimeFormat(
    'en', { minute: "numeric", second: "numeric", fractionalSecondDigits: undefined});
assert.sameValue(dtf.formatRange(d1, d2), "02:03", "no fractionalSecondDigits");
assert.sameValue(dtf.formatRange(d1, d3), "02:03 – 02:13", "no fractionalSecondDigits");

dtf = new Intl.DateTimeFormat(
    'en', { minute: "numeric", second: "numeric", fractionalSecondDigits: 1});
assert.sameValue(dtf.formatRange(d1, d2), "02:03.2 – 02:03.5", "1 fractionalSecondDigits round down");
assert.sameValue(dtf.formatRange(d1, d3), "02:03.2 – 02:13.9", "1 fractionalSecondDigits round down");

dtf = new Intl.DateTimeFormat(
    'en', { minute: "numeric", second: "numeric", fractionalSecondDigits: 2});
assert.sameValue(dtf.formatRange(d1, d2), "02:03.23 – 02:03.56", "2 fractionalSecondDigits round down");
assert.sameValue(dtf.formatRange(d1, d3), "02:03.23 – 02:13.98", "2 fractionalSecondDigits round down");

dtf = new Intl.DateTimeFormat(
    'en', { minute: "numeric", second: "numeric", fractionalSecondDigits: 3});
assert.sameValue(dtf.formatRange(d1, d2), "02:03.234 – 02:03.567", "3 fractionalSecondDigits round down");
assert.sameValue(dtf.formatRange(d1, d3), "02:03.234 – 02:13.987", "3 fractionalSecondDigits round down");
