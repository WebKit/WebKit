// This file was procedurally generated from the following sources:
// - src/declarations/class.case
// - src/declarations/redeclare/switch-attempt-to-redeclare-generator-declaration.template
/*---
description: redeclaration with ClassDeclaration (GeneratorDeclaration in SwitchStatement)
esid: sec-switch-statement-static-semantics-early-errors
features: [generators]
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

switch (0) { case 1: function* f() {} default: class f {} }
