// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale
description: >
    The value of the [[Prototype]] internal slot of the Intl.Locale constructor is the
    intrinsic object %FunctionPrototype%.
info: |
  The value of Intl.Locale.prototype is %LocalePrototype%.

  This property has the attributes
  { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.

includes: [propertyHelper.js]
features: [Intl.Locale]
---*/

assert.sameValue(typeof Intl.Locale, "function", "typeof Intl.Locale is function");

verifyProperty(Intl, "Locale", {
  value: Intl.Locale,
  writable: false,
  enumerable: false,
  configurable: false,
});
