// This file was procedurally generated from the following sources:
// - src/statementList/let-declaration.case
// - src/statementList/default/eval-class-declaration.template
/*---
description: LexicalDeclaration using Let (Valid syntax of StatementList starting with a Class Declaration)
esid: prod-StatementList
features: [class]
flags: [generated]
info: |
    StatementList:
      StatementListItem
      StatementList StatementListItem

    StatementListItem:
      Statement
      Declaration

    Declaration:
      ClassDeclaration


    Declaration:
      LexicalDeclaration

    LexicalDeclaration:
      LetOrConst BindingList ;

    BindingList:
      LexicalBinding
      BindingList , LexicalBinding
---*/


var result = eval('class C {}let a, b = 42, c;b;');

assert.sameValue(result, 42);
