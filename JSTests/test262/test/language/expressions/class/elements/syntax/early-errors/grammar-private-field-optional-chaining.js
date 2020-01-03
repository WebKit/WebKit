// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-private-field-optional-chaining.case
// - src/class-elements/syntax/invalid/cls-expr-elements-invalid-syntax.template
/*---
description: PrivateName after '?.' is not valid syntax (class expression)
esid: prod-ClassElement
features: [class-fields-private, optional-chaining, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Updated Productions

    MemberExpression[Yield]:
      MemberExpression[?Yield].PrivateName

---*/


$DONOTEVALUATE();

var C = class {
  #m = 'test262';

  access(obj) {
    return obj?.#m;
  }
};
