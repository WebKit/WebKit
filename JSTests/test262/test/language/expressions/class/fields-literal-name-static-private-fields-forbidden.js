// This file was procedurally generated from the following sources:
// - src/class-fields/static-private-fields-forbidden.case
// - src/class-fields/propname-error-static/cls-expr-static-literal-name.template
/*---
description: static private class fields forbidden (early error -- PropName of IdentifierName is forbidden)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-private]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
---*/


throw "Test262: This statement should not be evaluated.";

var C = class {
  static #field;
};
