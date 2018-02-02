// This file was procedurally generated from the following sources:
// - src/class-fields/init-err-contains-super.case
// - src/class-fields/initializer-error/cls-expr-fields-comp-name.template
/*---
description: Syntax error if `super()` used in class field (computed ClassElementName)
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


throw "Test262: This statement should not be evaluated.";

var x = "string";
var C = class {
  [x] = super();
}
