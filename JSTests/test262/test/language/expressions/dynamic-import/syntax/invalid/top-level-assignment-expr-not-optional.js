// This file was procedurally generated from the following sources:
// - src/dynamic-import/assignment-expr-not-optional.case
// - src/dynamic-import/syntax/invalid/top-level.template
/*---
description: It's a SyntaxError if AssignmentExpression is omitted (top level syntax)
esid: sec-import-call-runtime-semantics-evaluation
features: [dynamic-import]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ImportCall :
        import( AssignmentExpression )


    ImportCall :
        import( AssignmentExpression[+In, ?Yield] )
---*/

$DONOTEVALUATE();

import();

/* The params region intentionally empty */
