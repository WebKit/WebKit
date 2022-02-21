// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.PluralRules.prototype.selectRange
description: >
  "selectRange" basic tests when argument  x > y, throw a RangeError exception.
info: |
  1.1.6 ResolvePluralRange ( pluralRules, x, y )
  (...)
  5. If x > y, throw a RangeError exception.
features: [Intl.NumberFormat-v3]
---*/

const pr = new Intl.PluralRules();

// 1. If x > y, throw a RangeError exception.
assert.throws(RangeError, () => { pr.selectRange(201, 102) });
