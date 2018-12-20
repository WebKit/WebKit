// Copyright (C) 2018 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-static-semantics-static-semantics-assignmenttargettype
description: >
  import.meta is not a valid assignment target.
info: |
  Static Semantics: AssignmentTargetType

    ImportMeta:
      import.meta

    Return invalid.

  12.15.1 Static Semantics: Early Errors

    AssignmentExpression : LeftHandSideExpression = AssignmentExpression

    It is an early Reference Error if LeftHandSideExpression is neither an ObjectLiteral nor an
    ArrayLiteral and AssignmentTargetType of LeftHandSideExpression is invalid.
flags: [module]
negative:
  phase: early
  type: ReferenceError
features: [import.meta]
---*/

$DONOTEVALUATE();

import.meta = 0;
