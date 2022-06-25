// This file was procedurally generated from the following sources:
// - src/class-elements/private-method-cannot-escape-token.case
// - src/class-elements/syntax/invalid/cls-expr-elements-invalid-syntax.template
/*---
description: The pound signal in the private method cannot be escaped (class expression)
esid: prod-ClassElement
features: [class-methods-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    PrivateName::
      # IdentifierName

    U+0023 is the escape sequence for #

---*/


$DONOTEVALUATE();

var C = class {
  \u0023m() { return 42; }
};
