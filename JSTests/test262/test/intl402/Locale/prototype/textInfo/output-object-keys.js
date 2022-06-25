// Copyright 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale.prototype.textInfo
description: >
    Checks that the return value of Intl.Locale.prototype.textInfo is an Object
    with the correct keys and properties.
info: |
  get Intl.Locale.prototype.textInfo
  ...
  7. Perform ! CreateDataPropertyOrThrow(info, "direction", dir).
features: [Intl.Locale,Intl.Locale-info]
includes: [propertyHelper.js, compareArray.js]
---*/

const result = new Intl.Locale('en').textInfo;

assert.compareArray(Reflect.ownKeys(result), ['direction']);

verifyProperty(result, 'direction', {
  writable: true,
  enumerable: true,
  configurable: true
});

const direction = new Intl.Locale('en').textInfo.direction;
assert(
  direction === 'rtl' || direction === 'ltr',
  'value of the `direction` property'
);
