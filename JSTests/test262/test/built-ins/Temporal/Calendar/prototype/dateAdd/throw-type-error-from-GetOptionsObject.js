// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd should throw from GetOptionsObject.
info: |
  ...
  6. Set options to ? GetOptionsObject(options).
features: [BigInt, Symbol, Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar('iso8601');
let invalidOptionsList = [null, 'invalid option', 234, 23n, Symbol('foo'), true, false, Infinity];

invalidOptionsList.forEach(function(invalidOptions) {
  assert.throws(
    TypeError,
    () => cal.dateAdd('2020-02-03', 'P1Y', invalidOptions),
    'cal.dateAdd("2020-02-03", "P1Y", invalidOptions) throws a TypeError exception'
  );
});
