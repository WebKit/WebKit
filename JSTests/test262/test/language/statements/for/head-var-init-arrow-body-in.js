// Copyright (C) 2026 itsu-dev. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  The [~In] restriction on a for statement variable initializer does not
  propagate into syntactic contexts where Expression is parameterized with
  [+In] inside an arrow function body.
info: |
  ForStatement :
    for ( var VariableDeclarationList[~In, ?Yield, ?Await] ;
          Expression[+In, ?Yield, ?Await]opt ;
          Expression[+In, ?Yield, ?Await]opt ) Statement

  The expressions occurring in a parenthesized expression, if, do-while,
  while, return, switch, throw, and nested for-in statements are parsed with
  the In grammar parameter enabled.
esid: sec-for-statement
features: [arrow-function]
---*/

for (var f = () => ("x" in {}); false;) {}

for (var f = () => {
  if ("x" in {}) {}
}; false;) {}

for (var f = () => {
  do {} while ("x" in {});
}; false;) {}

for (var f = () => {
  while ("x" in {}) {}
}; false;) {}

for (var f = () => {
  for (var x in {}) {}
}; false;) {}

for (var f = () => {
  for (var g = () => {
    return "x" in {};
  }; false;) {}
}; false;) {}

for (var f = () => {
  return "x" in {};
}; false;) {}

for (var f = () => {
  switch ("x" in {}) {
    case "a" in {}:
      break;
  }
}; false;) {}

for (var f = () => {
  throw "x" in {};
}; false;) {}
