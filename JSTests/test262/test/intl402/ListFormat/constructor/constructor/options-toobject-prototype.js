// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat
description: Checks handling of non-object option arguments to the ListFormat constructor.
info: |
    InitializeListFormat (listFormat, locales, options)
features: [Intl.ListFormat]
---*/

Object.defineProperties(Object.prototype, {
  "type": {
    value: "unit",
  },
  "style": {
    value: "short",
  },
})

const optionsArguments = [
  true,
  "test",
  7,
  Symbol(),
];

for (const options of optionsArguments) {
  const lf = new Intl.ListFormat([], options);
  const resolvedOptions = lf.resolvedOptions();
  assert.sameValue(resolvedOptions.type, "unit",
    `options argument ${String(options)} should yield the correct value for "type"`);
  assert.sameValue(resolvedOptions.style, "short",
    `options argument ${String(options)} should yield the correct value for "style"`);
}
