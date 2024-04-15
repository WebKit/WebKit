// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.tojson
description: Balancing the maximum nanoseconds and seconds does not go out of range
features: [Temporal]
---*/

const d = new Temporal.Duration(0, 0, 0, 0, 0, 0, /* s = */ Number.MAX_SAFE_INTEGER, 0, 0, /* ns = */ 999_999_999);
assert.sameValue(d.toJSON(), "PT9007199254740991.999999999S", "max value ns and s does not go out of range");
