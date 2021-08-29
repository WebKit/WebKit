// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Temporal.Calendar.prototype.dateFromFields should throw Error from ISODateFromFields.
info: |
  6. Let result be ? ISODateFromFields(fields, options).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601")

assert.throws(RangeError, () => cal.dateFromFields({year: 2021, month: 7, day: 20},
      {overflow: "invalid garbage"}),
    'cal.dateFromFields({year: 2021, month: 7, day: 20}, {overflow: "invalid garbage"}) throws a RangeError exception');
