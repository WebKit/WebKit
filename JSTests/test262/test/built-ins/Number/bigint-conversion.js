// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: BigInt to Number conversion
esid: pending
features: [BigInt]
---*/

assert.sameValue(Number(0n), 0);
assert.sameValue(+(new Number(0n)), +(new Number(0)));
