// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: Number for fractionalSecondDigits option
features: [Temporal]
---*/

const t1 = Temporal.PlainTime.from("15:23");
const t2 = Temporal.PlainTime.from("15:23:30");
const t3 = Temporal.PlainTime.from("15:23:30.1234");
assert.sameValue(t3.toString({ fractionalSecondDigits: 0 }), "15:23:30");
assert.sameValue(t1.toString({ fractionalSecondDigits: 2 }), "15:23:00.00");
assert.sameValue(t2.toString({ fractionalSecondDigits: 2 }), "15:23:30.00");
assert.sameValue(t3.toString({ fractionalSecondDigits: 2 }), "15:23:30.12");
assert.sameValue(t3.toString({ fractionalSecondDigits: 3 }), "15:23:30.123");
assert.sameValue(t3.toString({ fractionalSecondDigits: 6 }), "15:23:30.123400");
assert.sameValue(t1.toString({ fractionalSecondDigits: 7 }), "15:23:00.0000000");
assert.sameValue(t2.toString({ fractionalSecondDigits: 7 }), "15:23:30.0000000");
assert.sameValue(t3.toString({ fractionalSecondDigits: 7 }), "15:23:30.1234000");
assert.sameValue(t3.toString({ fractionalSecondDigits: 9 }), "15:23:30.123400000");
