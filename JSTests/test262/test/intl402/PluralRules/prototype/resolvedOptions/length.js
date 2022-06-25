// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.pluralrules.prototype.resolvedoptions
description: >
  Intl.PluralRules.prototype.resolvedOptions.length is 0.
info: |
  Intl.PluralRules.prototype.resolvedOptions ()

  17 ECMAScript Standard Built-in Objects:

    Every built-in function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this
    value is equal to the largest number of named arguments shown in the
    subclause headings for the function description. Optional parameters
    (which are indicated with brackets: [ ]) or rest parameters (which
    are shown using the form «...name») are not included in the default
    argument count.
    Unless otherwise specified, the length property of a built-in function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.

includes: [propertyHelper.js]
---*/

assert.sameValue(Intl.PluralRules.prototype.resolvedOptions.length, 0);

verifyNotEnumerable(Intl.PluralRules.prototype.resolvedOptions, "length");
verifyNotWritable(Intl.PluralRules.prototype.resolvedOptions, "length");
verifyConfigurable(Intl.PluralRules.prototype.resolvedOptions, "length");
