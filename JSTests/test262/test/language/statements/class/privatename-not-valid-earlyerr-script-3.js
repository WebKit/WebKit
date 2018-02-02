// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-scripts-static-semantics-early-errors
description: Early error when referencing privatename that has not been declared in class.
info: |
  Static Semantics: Early Errors
    Script : ScriptBody
    1. Let names be an empty List.
      ...
    3. If AllPrivateNamesValid of ScriptBody with the argument names is false, throw a SyntaxError exception.
features: [class, class-fields-private, class-fields-public]
negative:
  phase: parse
  type: SyntaxError
---*/

throw "Test262: This statement should not be evaluated.";

class C {
  y = this.#x;
}
