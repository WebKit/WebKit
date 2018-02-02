// This file was procedurally generated from the following sources:
// - src/class-fields/static-private-fields-forbidden.case
// - src/class-fields/propname-error-static/cls-decl-static-literal-name.template
/*---
description: static private class fields forbidden (early error -- PropName of IdentifierName is forbidden value)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-private]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
---*/


throw "Test262: This statement should not be evaluated.";

class C {
  static #field;
}
