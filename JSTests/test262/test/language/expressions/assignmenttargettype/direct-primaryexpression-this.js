// This file was procedurally generated from the following sources:
// - src/assignment-target-type/primaryexpression-this.case
// - src/assignment-target-type/invalid/direct.template
/*---
description: PrimaryExpression this; Return invalid. (Direct assignment)
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Direct assignment
---*/

$DONOTEVALUATE();

this = 1;
