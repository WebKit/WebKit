// Copyright 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.Locale.prototype.numberingSystems
description: >
    Verifies the branding check for the "numberingSystems" property of the Locale prototype object.
info: |
    Intl.Locale.prototype.numberingSystems

    2. If Type(loc) is not Object or loc does not have an [[InitializedLocale]] internal slot, then
        a. Throw a TypeError exception.
features: [Intl.Locale,Intl.Locale-info]
---*/

const propdesc = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, "numberingSystems");
assert.sameValue(typeof propdesc.get, "function");
const invalidValues = [
  undefined,
  null,
  true,
  "",
  Symbol(),
  1,
  {},
  Intl.Locale.prototype,
];

for (const invalidValue of invalidValues) {
  assert.throws(TypeError, () => propdesc.get.call(invalidValue));
}
