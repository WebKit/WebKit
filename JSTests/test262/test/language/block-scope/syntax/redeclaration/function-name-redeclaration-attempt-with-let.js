// This file was procedurally generated from the following sources:
// - src/declarations/let.case
// - src/declarations/redeclare/block-attempt-to-redeclare-function-declaration.template
/*---
description: redeclaration with let-LexicalDeclaration (FunctionDeclaration in BlockStatement)
esid: sec-block-static-semantics-early-errors
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Block : { StatementList }

    It is a Syntax Error if the LexicallyDeclaredNames of StatementList contains
    any duplicate entries.

---*/


$DONOTEVALUATE();

{ function f() {} let f }
