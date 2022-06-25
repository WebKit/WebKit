// This file was procedurally generated from the following sources:
// - src/declarations/const.case
// - src/declarations/redeclare/switch-attempt-to-redeclare-function-declaration.template
/*---
description: redeclaration with const-LexicalDeclaration (FunctionDeclaration in SwitchStatement)
esid: sec-switch-statement-static-semantics-early-errors
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    SwitchStatement : switch ( Expression ) CaseBlock

    It is a Syntax Error if the LexicallyDeclaredNames of CaseBlock contains any
    duplicate entries.

---*/


$DONOTEVALUATE();

switch (0) { case 1: function f() {} default: const f = 0 }
