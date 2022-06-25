/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function obj() {
  var o = {all: 1, nowrite: 1, noconfig: 1, noble: 1};
  Object.defineProperty(o, 'nowrite', {writable: false});
  Object.defineProperty(o, 'noconfig', {configurable: false});
  Object.defineProperty(o, 'noble', {writable: false, configurable: false});
  return o;
}

assertEq(testLenientAndStrict('var o = obj(); o.all = 2; o.all',
                              returns(2), returns(2)),
         true);

assertEq(testLenientAndStrict('var o = obj(); o.nowrite = 2; o.nowrite',
                              returns(1), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var o = obj(); o.noconfig = 2; o.noconfig',
                              returns(2), returns(2)),
         true);

assertEq(testLenientAndStrict('var o = obj(); o.noble = 2; o.noble',
                              returns(1), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('obj().nowrite++',
                              returns(1), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('++obj().nowrite',
                              returns(2), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('obj().nowrite--',
                              returns(1), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('--obj().nowrite',
                              returns(0), raisesException(TypeError)),
         true);


function arr() {
  return Object.defineProperties([1, 1, 1, 1],
                                 { 1: { writable: false },
                                   2: { configurable: false },
                                   3: { writable: false, configurable: false }});
}

assertEq(testLenientAndStrict('var a = arr(); a[0] = 2; a[0]',
                              returns(2), returns(2)),
         true);

assertEq(testLenientAndStrict('var a = arr(); a[1] = 2; a[1]',
                              returns(1), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var a = arr(); a[2] = 2; a[2]',
                              returns(2), returns(2)),
         true);

assertEq(testLenientAndStrict('var a = arr(); a[3] = 2; a[3]',
                              returns(1), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('arr()[1]++',
                              returns(1), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('++arr()[1]',
                              returns(2), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('arr()[1]--',
                              returns(1), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('--arr()[1]',
                              returns(0), raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
