// This file was procedurally generated from the following sources:
// - src/statementList/block-with-labels.case
// - src/statementList/default/eval-function-declaration.template
/*---
description: Block with a label (Eval production of StatementList starting with a Function Declaration)
esid: prod-StatementList
flags: [generated]
info: |
    StatementList:
      StatementListItem
      StatementList StatementListItem

    StatementListItem:
      Statement
      Declaration

    Declaration:
      HoistableDeclaration

    FunctionDeclaration:
      function BindingIdentifier ( FormalParameters ) { FunctionBody }

    Statement:
      BlockStatement
      VariableStatement
      EmptyStatement
      ExpressionStatement
      ...

    // lookahead here prevents capturing an Object literal
    ExpressionStatement:
      [lookahead ∉ { {, function, async [no LineTerminator here] function, class, let [ }]
        Expression ;
---*/


var result = eval('function fn() {}{x: 42};');

assert.sameValue(result, 42, 'it does not evaluate to an Object with the property x');
