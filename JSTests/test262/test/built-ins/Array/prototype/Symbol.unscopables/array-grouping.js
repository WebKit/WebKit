// Copyright (C) 2022 Chengzhong Wu. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype-@@unscopables
description: >
    Initial value of `Symbol.unscopables` property
info: |
    22.1.3.32 Array.prototype [ @@unscopables ]

    ...
    10. Perform ! CreateDataPropertyOrThrow(unscopableList, "groupBy", true).
    11. Perform ! CreateDataPropertyOrThrow(unscopableList, "groupByToMap", true).
    ...

includes: [propertyHelper.js]
features: [Symbol.unscopables, array-grouping]
---*/

var unscopables = Array.prototype[Symbol.unscopables];

assert.sameValue(Object.getPrototypeOf(unscopables), null);

assert.sameValue(unscopables.groupBy, true, '`groupBy` property value');
verifyEnumerable(unscopables, 'groupBy');
verifyWritable(unscopables, 'groupBy');
verifyConfigurable(unscopables, 'groupBy');

assert.sameValue(unscopables.groupByToMap, true, '`groupByToMap` property value');
verifyEnumerable(unscopables, 'groupByToMap');
verifyWritable(unscopables, 'groupByToMap');
verifyConfigurable(unscopables, 'groupByToMap');
