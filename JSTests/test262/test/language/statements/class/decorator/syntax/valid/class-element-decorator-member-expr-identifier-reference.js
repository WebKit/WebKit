// This file was procedurally generated from the following sources:
// - src/decorator/decorator-member-expr-identifier-reference.case
// - src/decorator/syntax/valid/cls-element-decorators-valid-syntax.template
/*---
description: Decorator @ DecoratorMemberExpression (Valid syntax for decorator on class.)
esid: prod-ClassDeclaration
features: [class, decorators]
flags: [generated]
info: |
    ClassElement[Yield, Await] :
      DecoratorList[?Yield, ?Await]opt MethodDefinition[?Yield, ?Await]
      DecoratorList[?Yield, ?Await]opt static MethodDefinition[?Yield, ?Await]
      DecoratorList[?Yield, ?Await]opt FieldDefinition[?Yield, ?Await] ;
      DecoratorList[?Yield, ?Await]opt static FieldDefinition[?Yield, ?Await] ;
      ClassStaticBlock
      ;

    DecoratorList[Yield, Await] :
      DecoratorList[?Yield, ?Await]opt Decorator[?Yield, ?Await]

    Decorator[Yield, Await] :
      @ DecoratorMemberExpression[?Yield, ?Await]
      @ DecoratorParenthesizedExpression[?Yield, ?Await]
      @ DecoratorCallExpression[?Yield, ?Await]

    ...


    IdentifierReference[Yield, Await] :
      Identifier
      [~Yield] yield
      [~Await] await

---*/
function $() {}
function _() {}
function \u{6F}() {}
function \u2118() {}
function ZW_\u200C_NJ() {}
function ZW_\u200D_J() {}
function await() {}



class C {
  @$
  @_
  @\u{6F}
  @\u2118
  @ZW_\u200C_NJ
  @ZW_\u200D_J
  @await method() {}
  @$
  @_
  @\u{6F}
  @\u2118
  @ZW_\u200C_NJ
  @ZW_\u200D_J
  @await static method() {}
  @$
  @_
  @\u{6F}
  @\u2118
  @ZW_\u200C_NJ
  @ZW_\u200D_J
  @await field;
  @$
  @_
  @\u{6F}
  @\u2118
  @ZW_\u200C_NJ
  @ZW_\u200D_J
  @await static field;
}
