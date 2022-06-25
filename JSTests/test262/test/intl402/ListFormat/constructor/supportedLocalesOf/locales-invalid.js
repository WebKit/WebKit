// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat.supportedLocalesOf
description: Checks error cases for the locales argument to the supportedLocalesOf function.
info: |
    Intl.ListFormat.supportedLocalesOf ( locales [, options ])

    2. Let requestedLocales be CanonicalizeLocaleList(locales).
includes: [testIntl.js]
features: [Intl.ListFormat]
---*/

assert.sameValue(typeof Intl.ListFormat.supportedLocalesOf, "function",
                 "Should support Intl.ListFormat.supportedLocalesOf.");

for (const [locales, expectedError] of getInvalidLocaleArguments()) {
    assert.throws(expectedError, () => Intl.ListFormat.supportedLocalesOf(locales));
}
