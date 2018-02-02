// This file was procedurally generated from the following sources:
// - src/declarations/redeclare-with-generator-declaration.case
// - src/declarations/redeclare/block-attempt-to-redeclare-function-declaration.template
/*---
description: redeclaration with GeneratorDeclaration (FunctionDeclaration in BlockStatement)
esid: sec-block-static-semantics-early-errors
features: [generators]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Block : { StatementList }

    It is a Syntax Error if the LexicallyDeclaredNames of StatementList contains
    any duplicate entries.

---*/


throw "Test262: This statement should not be evaluated.";

{ function f() {} function* f() {} }
