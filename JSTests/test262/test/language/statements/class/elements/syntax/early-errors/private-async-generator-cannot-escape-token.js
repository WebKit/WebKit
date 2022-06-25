// This file was procedurally generated from the following sources:
// - src/class-elements/private-async-generator-cannot-escape-token.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: The pound signal in the private async generator cannot be escaped (class declaration)
esid: prod-ClassElement
features: [class-methods-private, async-iteration, class]
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

class C {
  async * \u0023m() { return 42; }
}
