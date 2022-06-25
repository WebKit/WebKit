// This file was procedurally generated from the following sources:
// - src/declarations/const.case
// - src/declarations/redeclare/fn-block-attempt-to-redeclare-var-declaration.template
/*---
description: redeclaration with const-LexicalDeclaration (VariableDeclaration in BlockStatement inside a function)
esid: sec-block-static-semantics-early-errors
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Block : { StatementList }

    It is a Syntax Error if any element of the LexicallyDeclaredNames of
    StatementList also occurs in the VarDeclaredNames of StatementList.

---*/


$DONOTEVALUATE();

function x() {
  { const f = 0; var f; }
}
