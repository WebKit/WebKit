// This file was procedurally generated from the following sources:
// - src/class-elements/eval-err-contains-arguments.case
// - src/class-elements/initializer-eval-arguments/cls-decl-fields-indirect-eval-nested.template
/*---
description: error if `arguments` in StatementList of eval (indirect eval)
esid: sec-performeval-rules-in-initializer
features: [class, class-fields-public]
flags: [generated]
info: |
    Additional Early Error Rules for Eval Inside Initializer
    These static semantics are applied by PerformEval when a direct eval call occurs inside a class field initializer.
    ScriptBody : StatementList

      It is a Syntax Error if ContainsArguments of StatementList is true.
      ...

    Static Semantics: ContainsArguments
      IdentifierReference : Identifier

      1. If the StringValue of Identifier is "arguments", return true.
      ...
      For all other grammatical productions, recurse on all nonterminals. If any piece returns true, then return true. Otherwise return false.

---*/


var executed = false;
class C {
  x = () => {
    var t = () => { (0, eval)('executed = true; arguments;'); };
    t();
  }
}

assert.throws(ReferenceError, function() {
  new C().x();
});

assert.sameValue(executed, false);
