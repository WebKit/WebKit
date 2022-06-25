// This file was procedurally generated from the following sources:
// - src/compound-assignment-private/sub.case
// - src/compound-assignment-private/default/data-property.template
/*---
description: Compound subtraction assignment with target being a private reference (to a field)
esid: sec-assignment-operators-runtime-semantics-evaluation
features: [class-fields-private]
flags: [generated]
info: |
    sec-assignment-operators-runtime-semantics-evaluation
    AssignmentExpression : LeftHandSideExpression AssignmentOperator AssignmentExpression
      1. Let _lref_ be the result of evaluating |LeftHandSideExpression|.
      2. Let _lval_ be ? GetValue(_lref_).
      ...
      7. Let _r_ be ApplyStringOrNumericBinaryOperator(_lval_, _opText_, _rval_).
      8. Perform ? PutValue(_lref_, _r_).
      9. Return _r_.

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

---*/


class C {
  #field = 3;
  compoundAssignment() {
    return this.#field -= 2;
  }
  fieldValue() {
    return this.#field;
  }
}

const o = new C();
assert.sameValue(o.compoundAssignment(), 1, "The expression should evaluate to the result");
assert.sameValue(o.fieldValue(), 1, "PutValue should store the result in the private reference");
