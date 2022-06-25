// This file was procedurally generated from the following sources:
// - src/logical-assignment-private/or.case
// - src/logical-assignment-private/default/data-property-short-circuit.template
/*---
description: Logical-or assignment with target being a private reference (to a field (short-circuit version))
esid: sec-assignment-operators-runtime-semantics-evaluation
features: [class-fields-private, logical-assignment-operators]
flags: [generated]
info: |
    sec-property-accessors-runtime-semantics-evaluation
    MemberExpression : MemberExpression `.` PrivateIdentifier

      1. Let _baseReference_ be the result of evaluating |MemberExpression|.
      2. Let _baseValue_ be ? GetValue(_baseReference_).
      3. Let _fieldNameString_ be the StringValue of |PrivateIdentifier|.
      4. Return ! MakePrivateReference(_baseValue_, _fieldNameString_).

    PutValue (V, W)
      ...
      5.b. If IsPrivateReference(_V_) is *true*, then
        i. Return ? PrivateSet(_baseObj_, _V_.[[ReferencedName]], _W_).

    PrivateSet (O, P, value)
      ...
      3. If _entry_.[[Kind]] is ~field~, then
        a. Set _entry_.[[Value]] to _value_.


    sec-assignment-operators-runtime-semantics-evaluation
    AssignmentExpression : LeftHandSideExpression ||= AssignmentExpression
      1. Let _lref_ be the result of evaluating |LeftHandSideExpression|.
      2. Let _lval_ be ? GetValue(_lref_).
      3. Let _lbool_ be ! ToBoolean(_lval_).
      4. If _lbool_ is *true*, return _lval_.
      ...
      7. Perform ? PutValue(_lref_, _rval_).
      8. Return _rval_.
---*/


function doNotCall() {
  throw new Test262Error("The right-hand side should not be evaluated");
}

class C {
  #field = true;
  compoundAssignment() {
    return this.#field ||= doNotCall();
  }
  fieldValue() {
    return this.#field;
  }
}

const o = new C();
assert.sameValue(o.compoundAssignment(), true, "The expression should evaluate to the short-circuit value");
