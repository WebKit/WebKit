// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 12.14.4
description: >
    Assignment of function `name` attribute (CoverParenthesizedExpression)
info: |
    AssignmentExpression[In, Yield] :
        LeftHandSideExpression[?Yield] = AssignmentExpression[?In, ?Yield]

    1. If LeftHandSideExpression is neither an ObjectLiteral nor an
       ArrayLiteral, then
       [...]
       e. If IsAnonymousFunctionDefinition(AssignmentExpression) and
          IsIdentifierRef of LeftHandSideExpression are both true, then

          i. Let hasNameProperty be HasOwnProperty(rval, "name").
          ii. ReturnIfAbrupt(hasNameProperty).
          iii. If hasNameProperty is false, perform SetFunctionName(rval,
               GetReferencedName(lref)).
includes: [propertyHelper.js]
---*/

var xCover, cover;

xCover = (0, function() {});
cover = (function() {});

assert(xCover.name !== 'xCover');

assert.sameValue(cover.name, 'cover');
verifyNotEnumerable(cover, 'name');
verifyNotWritable(cover, 'name');
verifyConfigurable(cover, 'name');
