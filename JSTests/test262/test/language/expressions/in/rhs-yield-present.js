// Copyright 2021 the V8 project authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Parsing observes the `Yield` production parameter when present
info: |
  Syntax
    RelationalExpression[In, Yield, Await]:
    [...]
    [+In] RelationalExpression[+In, ?Yield, ?Await] in ShiftExpression[?Yield, ?Await]

  [...]

  1. Let lref be the result of evaluating RelationalExpression.
  2. Let lval be ? GetValue(lref).
  3. Let rref be the result of evaluating ShiftExpression.
  4. Let rval be ? GetValue(rref).
  5. If Type(rval) is not Object, throw a TypeError exception.
  6. Return ? HasProperty(rval, ? ToPropertyKey(lval)).
esid: sec-relational-operators-runtime-semantics-evaluation
---*/

function * isNameIn() {
  return '' in (yield);
}

let iter1 = isNameIn();
iter1.next();
assert.sameValue(iter1.next({'': 0}).value, true);

let iter2 = isNameIn();
iter2.next();
assert.sameValue(iter2.next({}).value, false);
