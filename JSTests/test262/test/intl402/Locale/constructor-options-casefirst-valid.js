// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    Checks valid cases for the options argument to the Locale constructor.
info: |
    Intl.Locale( tag [, options] )

    ...
    22. Let kf be ? GetOption(options, "caseFirst", "string", « "upper", "lower", "false" », undefined).
    23. Set opt.[[kf]] to kf.
    ...
    30. Let r be ! ApplyUnicodeExtensionToTag(tag, opt, relevantExtensionKeys).
    ...

    ApplyUnicodeExtensionToTag( tag, options, relevantExtensionKeys )

    ...
    8. Let locale be the String value that is tag with all Unicode locale extension sequences removed.
    9. Let newExtension be ! CanonicalizeUnicodeExtension(attributes, keywords).
    10. If newExtension is not the empty String, then
        a. Let locale be ! InsertUnicodeExtension(locale, newExtension).
    ...

    CanonicalizeUnicodeExtension( attributes, keywords )
    ...
    4. Repeat for each element entry of keywords in List order,
        a. Let keyword be entry.[[Key]].
        b. If entry.[[Value]] is not the empty String, then
            i. Let keyword be the string-concatenation of keyword, "-", and entry.[[Value]].
        c. Append keyword to fullKeywords.
    ...
features: [Intl.Locale]
---*/

const validCaseFirstOptions = [
  "upper",
  "lower",
  "false",
  false,
  { toString() { return false; } },
];
for (const caseFirst of validCaseFirstOptions) {
  const options = { caseFirst };
  const expected = String(caseFirst);
  assert.sameValue(
    new Intl.Locale('en', options).toString(),
    "en-u-kf-" + expected,
  );

  assert.sameValue(
    new Intl.Locale('en-u-kf-lower', options).toString(),
    "en-u-kf-" + expected,
  );

  if ("caseFirst" in Intl.Locale.prototype) {
    assert.sameValue(
      new Intl.Locale('en-u-kf-lower', options).caseFirst,
      expected,
    );
  }
}
