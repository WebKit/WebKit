// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat
description: Checks handling of valid values for the style option to the ListFormat constructor.
info: |
    InitializeListFormat (listFormat, locales, options)
    9. Let s be ? GetOption(options, "style", "string", «"long", "short", "narrow"», "long").
    10. Set listFormat.[[Style]] to s.
    14. If style is "narrow" and type is not "unit", throw a RangeError exception.
features: [Intl.ListFormat]
---*/

const validOptions = [
  [undefined, "long"],
  ["long", "long"],
  ["short", "short"],
  [{ toString() { return "short"; } }, "short"],
];

for (const [validOption, expected] of validOptions) {
  const lf = new Intl.ListFormat([], {"style": validOption});
  const resolvedOptions = lf.resolvedOptions();
  assert.sameValue(resolvedOptions.style, expected);
}

const lf = new Intl.ListFormat([], {"style": "narrow", "type": "unit"});
const resolvedOptions = lf.resolvedOptions();
assert.sameValue(resolvedOptions.style, "narrow");

assert.throws(RangeError, () => lf = new Intl.ListFormat([], {"style": "narrow"}));
assert.throws(RangeError, () => lf = new Intl.ListFormat([], {"style": "narrow", "type": "conjuction"}));
assert.throws(RangeError, () => lf = new Intl.ListFormat([], {"style": "narrow", "type": "disjuction"}));
