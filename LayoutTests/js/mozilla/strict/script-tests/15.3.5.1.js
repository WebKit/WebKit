/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function fn() {
  return function(a, b, c) { };
}

assertEq(testLenientAndStrict('var f = fn(); f.length = 1; f.length',
                              returns(3), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var f = fn(); delete f.length',
                              returns(true), returns(true)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
