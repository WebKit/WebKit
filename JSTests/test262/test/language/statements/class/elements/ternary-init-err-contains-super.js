// This file was procedurally generated from the following sources:
// - src/class-elements/init-err-contains-super.case
// - src/class-elements/initializer-error/cls-decl-fields-ternary.template
/*---
description: Syntax error if `super()` used in class field (ternary expression)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Static Semantics: Early Errors

      FieldDefinition:
        PropertyNameInitializeropt

      - It is a Syntax Error if Initializer is present and Initializer Contains SuperCall is true.

---*/


$DONOTEVALUATE();

class C {
  x = false ? {} : super();
}
