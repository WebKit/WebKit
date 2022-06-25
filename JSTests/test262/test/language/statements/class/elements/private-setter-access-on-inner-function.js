// This file was procedurally generated from the following sources:
// - src/class-elements/private-setter-access-on-inner-function.case
// - src/class-elements/default/cls-decl.template
/*---
description: PrivateName of private setter is visible on inner function of class scope (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class-methods-private, class]
flags: [generated]
info: |
    Updated Productions

    CallExpression[Yield, Await]:
      CoverCallExpressionAndAsyncArrowHead[?Yield, ?Await]
      SuperCall[?Yield, ?Await]
      CallExpression[?Yield, ?Await]Arguments[?Yield, ?Await]
      CallExpression[?Yield, ?Await][Expression[+In, ?Yield, ?Await]]
      CallExpression[?Yield, ?Await].IdentifierName
      CallExpression[?Yield, ?Await]TemplateLiteral[?Yield, ?Await]
      CallExpression[?Yield, ?Await].PrivateName

---*/


class C {
  set #m(v) { this._v = v; }

  method() {
    let self = this;
    function innerFunction() {
      self.#m = 'Test262';
    }

    innerFunction();
  }
}

let c = new C();
c.method();
assert.sameValue(c._v, 'Test262');
let o = {};
assert.throws(TypeError, function() {
  c.method.call(o);
}, 'accessed private setter from an ordinary object');
