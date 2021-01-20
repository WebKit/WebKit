// This file was procedurally generated from the following sources:
// - src/assignment-target-type/callexpression-arguments.case
// - src/assignment-target-type/invalid/parenthesized.template
/*---
description: Static Semantics AssignmentTargetType, Return invalid. (ParenthesizedExpression)
esid: sec-grouping-operator-static-semantics-assignmenttargettype
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ParenthesizedExpression: (Expression)

    Return AssignmentTargetType of Expression.

    CallExpression Arguments
    Static Semantics AssignmentTargetType, Return invalid.

---*/

$DONOTEVALUATE();

function _() {
  (f()) = 1;
}

