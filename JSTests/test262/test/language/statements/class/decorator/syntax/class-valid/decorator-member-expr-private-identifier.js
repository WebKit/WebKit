// This file was procedurally generated from the following sources:
// - src/decorator/decorator-member-expr-private-identifier.case
// - src/decorator/syntax/class-valid/cls-decl-decorators-valid-syntax.template
/*---
description: Decorator @ DecoratorMemberExpression (Valid syntax for decorator on class declaration in class body.)
esid: prod-ClassDeclaration
features: [class, decorators]
flags: [generated]
info: |
    ClassDeclaration[Yield, Await, Default] :
      DecoratorList[?Yield, ?Await]opt class BindingIdentifier[?Yield, ?Await] ClassTail[?Yield, ?Await]
      [+Default] DecoratorList[?Yield, ?Await]opt class ClassTail[?Yield, ?Await]

    DecoratorList[Yield, Await] :
      DecoratorList[?Yield, ?Await]opt Decorator[?Yield, ?Await]

    Decorator[Yield, Await] :
      @ DecoratorMemberExpression[?Yield, ?Await]
      @ DecoratorParenthesizedExpression[?Yield, ?Await]
      @ DecoratorCallExpression[?Yield, ?Await]

    ...


    DecoratorMemberExpression[Yield, Await] :
      PrivateIdentifier

    PrivateIdentifier ::
      # IdentifierName

---*/


class C {
  static #$() {}
  static #_() {}
  static #\u{6F}() {}
  static #\u2118() {}
  static #ZW_\u200C_NJ() {}
  static #ZW_\u200D_J() {}
  static #yield() {}
  static #await() {}

  static {
    @#$
    @#_
    @#\u{6F}
    @#\u2118
    @#ZW_\u200C_NJ
    @#ZW_\u200D_J
    @#yield
    @#await class C {}
  }
}
