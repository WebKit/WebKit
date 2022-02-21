// This file was procedurally generated from the following sources:
// - src/statementList/expr-arrow-function-boolean-literal.case
// - src/statementList/default/eval-function-declaration.template
/*---
description: Expression with an Arrow Function and Boolean literal (Eval production of StatementList starting with a Function Declaration)
esid: prod-StatementList
features: [arrow-function]
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

    ExpressionStatement:
      [lookahead ∉ { {, function, async [no LineTerminator here] function, class, let [ }]
        Expression ;

    Expression:
      AssignmentExpression
      Expression , AssignmentExpression

    AssignmentExpression:
      ConditionalExpression
      [+Yield]YieldExpression
      ArrowFunction

    ArrowFunction:
      ArrowParameters [no LineTerminator here] => ConciseBody

    ConciseBody:
      [lookahead ≠ {] AssignmentExpression
      { FunctionBody }

---*/


var result = eval('function fn() {}() => 1, 42;');

assert.sameValue(result, 42);
