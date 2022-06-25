// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.from
description: Temporal.Instant string with sub-minute offset
features: [Temporal]
---*/

const str = "1970-01-01T00:19:32.37+00:19:32.37";
const result = Temporal.Instant.from(str);
assert.sameValue(result.epochNanoseconds, 0n, "if present, sub-minute offset is accepted exactly");
