/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function arr() {
  return Object.defineProperty([1, 2, 3], 2, {configurable: false});
}

function obj() {
  var o = {0: 1, 1: 2, 2: 3, length: 3};
  Object.defineProperty(o, 2, {configurable: false});
  return o;
}

assertEq(testLenientAndStrict('var a = arr(); [a.pop(), a]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var o = obj(); [Array.prototype.pop.call(o), o]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
