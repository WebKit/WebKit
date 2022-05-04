// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.equals
description: Temporal.Instant string with sub-minute offset
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);

const str = "1970-01-01T00:19:32.37+00:19:32.37";
const result = instance.equals(str);
assert.sameValue(result, true, "if present, sub-minute offset is accepted exactly");
