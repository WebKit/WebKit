// This file was procedurally generated from the following sources:
// - src/assignment-target-type/new-memberexpression-arguments.case
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

    new MemberExpression Arguments
    Static Semantics AssignmentTargetType, Return invalid.

---*/

$DONOTEVALUATE();

(new f()) = 1;
