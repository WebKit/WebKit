// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.compare() does not lose precision when totaling everything down to nanoseconds
features: [Temporal]
---*/

assert.notSameValue(Temporal.Duration.compare({ days: 200 }, {
  days: 200,
  nanoseconds: 1
}), 0);
