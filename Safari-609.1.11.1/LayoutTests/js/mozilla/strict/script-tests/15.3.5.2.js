/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function fn() {
  return function(a, b, c) { };
}

assertEq(testLenientAndStrict('var f = fn(); delete f.prototype',
                              returns(false), raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
