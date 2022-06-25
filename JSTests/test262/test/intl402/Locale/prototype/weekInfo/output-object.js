// Copyright 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.locale.prototype.weekInfo
description: >
    Checks that the return value of Intl.Locale.prototype.weekInfo is an Object.
info: |
  get Intl.Locale.prototype.weekInfo
  ...
  5. Let info be ! ObjectCreate(%Object.prototype%).
features: [Intl.Locale,Intl.Locale-info]
---*/

assert.sameValue(Object.getPrototypeOf(new Intl.Locale('en').weekInfo), Object.prototype);
