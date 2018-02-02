// This file was procedurally generated from the following sources:
// - src/class-fields/member-expression-privatename.case
// - src/class-fields/delete-error/cls-expr-method-delete.template
/*---
description: Syntax error if you call delete on member expressions . privatename (in method)
esid: sec-class-definitions-static-semantics-early-errors
features: [class-fields-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Static Semantics: Early Errors

      UnaryExpression : delete UnaryExpression

      It is a Syntax Error if the UnaryExpression is contained in strict mode code and the derived UnaryExpression is PrimaryExpression : IdentifierReference , MemberExpression : MemberExpression.PrivateName , or CallExpression : CallExpression.PrivateName .

---*/


throw "Test262: This statement should not be evaluated.";

var C = class {
  #x;

  x() {
    
    delete this.#x;
  }

  
}
