/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function arr() {
  return Object.defineProperty([1, 2, 3], 0, {writable: false});
}

function obj() {
  var o = {0: 1, 1: 2, 2: 3, length: 3};
  Object.defineProperty(o, 0, {writable: false});
  return o;
}

assertEq(testLenientAndStrict('var a = arr(); [a.unshift(40, 50), a]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var o = obj(); [Array.prototype.unshift.call(o, 40, 50), o]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);

function agap() {
  var a = [1, 2, , 4];
  Object.defineProperty(a, 3, {configurable: false});
  return a;
}

function ogap() {
  var o = {0: 1, 1: 2, /* no 2 */ 3: 4, length: 4};
  Object.defineProperty(o, 3, {configurable: false});
  return o;
}

assertEq(testLenientAndStrict('var a = agap(); [a.unshift(9), a]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var o = ogap(); [Array.prototype.unshift.call(o, 9), o]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
