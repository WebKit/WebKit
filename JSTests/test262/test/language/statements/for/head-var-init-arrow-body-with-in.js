// Copyright (C) 2026 itsu-dev. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  The [~In] restriction on a for statement variable initializer does not
  propagate into the expression of a with statement inside an arrow function
  body.
info: |
  ForStatement :
    for ( var VariableDeclarationList[~In, ?Yield, ?Await] ;
          Expression[+In, ?Yield, ?Await]opt ;
          Expression[+In, ?Yield, ?Await]opt ) Statement

  WithStatement :
    with ( Expression[+In, ?Yield, ?Await] ) Statement
esid: sec-for-statement
features: [arrow-function]
flags: [noStrict]
---*/

for (var f = () => {
  with ("x" in {}) {}
}; false;) {}
