// This file was procedurally generated from the following sources:
// - src/declarations/async-function.case
// - src/declarations/redeclare/block-attempt-to-redeclare-inner-block-var-declaration.template
/*---
description: redeclaration with AsyncFunctionDeclaration (VariableDeclaration in a BlockStatement inside a BlockStatement)
esid: sec-block-static-semantics-early-errors
features: [async-functions]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Block : { StatementList }

    It is a Syntax Error if any element of the LexicallyDeclaredNames of
    StatementList also occurs in the VarDeclaredNames of StatementList.

    Static Semantics: VarDeclaredNames

    Block : { }

    1. Return a new empty List.

    StatementList : StatementList StatementListItem

    1. Let names be VarDeclaredNames of StatementList.
    2. Append to names the elements of the VarDeclaredNames of StatementListItem.
    3. Return names.

    StatementListItem : Declaration

    1. Return a new empty List.

---*/


$DONOTEVALUATE();

{ { var f; } async function f() {}; }
