// Copyright 2013-2019 Mozilla Corporation; Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-bigint.prototype.tolocalestring
description: >
    Tests that Number.prototype.toLocaleString uses the standard
    built-in Intl.NumberFormat constructor.
includes: [testIntl.js]
features: [BigInt]
---*/

taintDataProperty(Intl, "NumberFormat");
0n.toLocaleString();
