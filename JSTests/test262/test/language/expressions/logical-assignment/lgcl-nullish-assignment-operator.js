// Copyright (c) 2020 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-assignment-operators-runtime-semantics-evaluation
description: Logical Nullish Assignment Operator
info: |
    AssignmentExpression:
      LeftHandSideExpression ??= AssignmentExpression

    1. Let lref be the result of evaluating LeftHandSideExpression.
    2. Let lval be ? GetValue(lref).
    3. If lval is neither undefined nor null, return lval.
    4. Let rref be the result of evaluating AssignmentExpression.
    5. Let rval be ? GetValue(rref).
    6. Perform ? PutValue(lref, rval).
    7. Return rval.
features: [logical-assignment-operators]

---*/

var value = undefined;
assert.sameValue(value ??= 1, 1, "(value ??= 1) === 1; where value = undefined");

value = null;
assert.sameValue(value ??= 1, 1, "(value ??= 1) === 1; where value = null");

value = false;
assert.sameValue(value ??= 1, false, "(value ??= 1) === false; where value = false");

value = 0;
assert.sameValue(value ??= 1, 0, "(value ??= 1) === 0; where value = 0");

value = -0;
assert.sameValue(value ??= 1, -0, "(value ??= 1) === -0; where value = -0");

value = NaN;
assert.sameValue(value ??= 1, NaN, "(value ??= 1) === NaN; where value = NaN");

value = "";
assert.sameValue(value ??= 1, "", '(value ??= 1) === "" where value = ""');



value = true;
assert.sameValue(value ??= 1, true, "(value ??= 1) === true; where value = true");

value = 2;
assert.sameValue(value ??= 1, 2, "(value ??= 1) === 2; where value = 2");

value = "test";
assert.sameValue(value ??= 1, "test", '(value ??= 1) === "test"; where value = "test"');

var sym = Symbol("");
value = sym;
assert.sameValue(value ??= 1, sym, "(value ??= 1) === Symbol(); where value = Symbol()");

var obj = {};
value = obj;
assert.sameValue(value ??= 1, obj, "(value ??= 1) === {}; where value = {}");
