// Copyright 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale.prototype.textInfo
description: >
    Checks that the return value of Intl.Locale.prototype.textInfo is an Object.
info: |
  get Intl.Locale.prototype.textInfo
  ...
  5. Let info be ! ObjectCreate(%Object.prototype%).
features: [Intl.Locale,Intl.Locale-info]
---*/

assert.sameValue(Object.getPrototypeOf(new Intl.Locale('en').textInfo), Object.prototype);
