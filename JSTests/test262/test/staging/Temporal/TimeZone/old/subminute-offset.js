// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: sub-minute offset
features: [Temporal]
---*/

var zone = new Temporal.TimeZone("+00:19:32");
var inst = Temporal.Instant.from("2000-01-01T12:00Z");
var dtm = Temporal.PlainDateTime.from("2000-01-01T12:00");
assert.sameValue(zone.getOffsetNanosecondsFor(inst), 1172000000000);
assert.sameValue(`${ zone.getInstantFor(dtm) }`, "2000-01-01T11:40:28Z");
