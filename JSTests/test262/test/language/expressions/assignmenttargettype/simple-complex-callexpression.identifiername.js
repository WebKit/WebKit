// This file was procedurally generated from the following sources:
// - src/assignment-target-type/callexpression.identifiername.case
// - src/assignment-target-type/simple/complex/default.template
/*---
description: Static Semantics AssignmentTargetType, Return simple (Simple Direct assignment)
flags: [generated]
info: |
    CallExpression . IdentifierName
    Static Semantics AssignmentTargetType, Return simple

---*/


let v = 'v';
let o = { [v]: 1, f() {} };
let f = () => o;

f().IdentifierName = 1;
