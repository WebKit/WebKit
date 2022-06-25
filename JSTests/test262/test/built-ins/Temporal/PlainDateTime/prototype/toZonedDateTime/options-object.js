// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Empty object may be used as options
features: [Temporal]
---*/

const dt = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102);

assert.sameValue(
  dt.toZonedDateTime("UTC", {}).epochNanoseconds,
  1572345998271986102n,
  "options may be an empty plain object"
);

assert.sameValue(
  dt.toZonedDateTime("UTC", () => {}).epochNanoseconds,
  1572345998271986102n,
  "options may be a function object"
);
