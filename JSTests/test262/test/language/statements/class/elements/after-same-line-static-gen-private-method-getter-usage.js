// This file was procedurally generated from the following sources:
// - src/class-elements/private-method-getter-usage.case
// - src/class-elements/productions/cls-decl-after-same-line-static-gen.template
/*---
description: PrivateName CallExpression usage (Accesor get method) (field definitions after a static generator in the same line)
esid: prod-FieldDefinition
features: [class-methods-private, generators, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
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
  static *m() { return 42; } get #m() { return 'test262'; };
  method() {
    return this.#m;
  }
}

var c = new C();

assert.sameValue(C.m().next().value, 42);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, "m"), false);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(c.method(), 'test262');
