// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.RelativeTimeFormat
description: Checks various cases for the locales argument to the RelativeTimeFormat constructor.
info: |
    InitializeRelativeTimeFormat (relativeTimeFormat, locales, options)
    3. Let _requestedLocales_ be ? CanonicalizeLocaleList(_locales_).
includes: [testIntl.js]
features: [Intl.RelativeTimeFormat]
---*/

const defaultLocale = new Intl.RelativeTimeFormat().resolvedOptions().locale;

const tests = [
  [undefined, defaultLocale, "undefined"],
  ["EN", "en", "Single value"],
  [[], defaultLocale, "Empty array"],
  [["en-GB-oed"], "en-GB", "Grandfathered"],
  [["x-private"], defaultLocale, "Private", ["lookup"]],
  [["en", "EN"], "en", "Duplicate value (canonical first)"],
  [["EN", "en"], "en", "Duplicate value (canonical last)"],
  [{ 0: "DE", length: 0 }, defaultLocale, "Object with zero length"],
  [{ 0: "DE", length: 1 }, "de", "Object with length"],
];

for (const [locales, expected, name, matchers = ["best fit", "lookup"]] of tests) {
  for (const matcher of matchers) {
    const rtf = new Intl.RelativeTimeFormat(locales, {localeMatcher: matcher});
    assert.sameValue(rtf.resolvedOptions().locale, expected, name);
  }
}
