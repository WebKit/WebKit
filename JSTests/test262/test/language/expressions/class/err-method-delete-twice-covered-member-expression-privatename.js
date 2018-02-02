// This file was procedurally generated from the following sources:
// - src/class-fields/member-expression-privatename.case
// - src/class-fields/delete-error/cls-expr-method-delete-twice-covered.template
/*---
description: Syntax error if you call delete on member expressions . privatename (in method, recursively covered)
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

      It is a Syntax Error if the derived UnaryExpression is PrimaryExpression : CoverParenthesizedExpressionAndArrowParameterList and CoverParenthesizedExpressionAndArrowParameterList ultimately derives a phrase that, if used in place of UnaryExpression, would produce a Syntax Error according to these rules. This rule is recursively applied.

---*/


throw "Test262: This statement should not be evaluated.";

var C = class {
  #x;

  x() {
    
    delete ((this.#x));
  }

  
}
