// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.tojson
description: TypeError thrown when toString property not present
features: [Temporal]
---*/

const tz = Temporal.TimeZone.from('UTC');
tz.toString = undefined;

assert.throws(TypeError, () => tz.toJSON());
