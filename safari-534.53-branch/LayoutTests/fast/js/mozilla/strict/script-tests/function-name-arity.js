/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function fn() {
  return function f(a, b, c) { };
}

assertEq(testLenientAndStrict('var f = fn(); f.name = "g"; f.name',
                              returns("f"), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var f = fn(); delete f.name',
                              returns(false), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var f = fn(); f.arity = 4; f.arity',
                              returns(3), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var f = fn(); delete f.arity',
                              returns(false), raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
