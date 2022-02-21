// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tojson
description: Basic tests for toJSON()
features: [Temporal]
---*/

const instantBeforeEpoch = Temporal.Instant.from("1963-02-13T10:36:29.123456789+01:00");
assert.sameValue(instantBeforeEpoch.toJSON(), "1963-02-13T09:36:29.123456789Z");

const instantAfterEpoch = Temporal.Instant.from("1976-11-18T15:23:30.123456789+01:00");
assert.sameValue(instantAfterEpoch.toJSON(), "1976-11-18T14:23:30.123456789Z");
assert.sameValue(instantAfterEpoch.toJSON("+01:00"), "1976-11-18T14:23:30.123456789Z",
  "after epoch with ignored argument");
