// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.equals
description: Time zone names are case insensitive
features: [Temporal]
---*/

const timeZone = 'UtC';
const result = Temporal.TimeZone.from(timeZone);
assert.sameValue(result.equals(timeZone), true);
assert.sameValue(result.equals("+00:00"), false);
