// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetime
description: Rejects when `getOffsetNanosecondsFor` property is not a method
features: [Temporal]
---*/

var timeZone = {
  getOffsetNanosecondsFor: 7
};

assert.throws(TypeError, function() {
  Temporal.Now.plainDateTime('iso8601', timeZone);
}, 'Temporal.Now.plainDateTime("iso8601", timeZone) throws a TypeError exception');
