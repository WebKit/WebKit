/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function arr() {
  return Object.defineProperty([1, 2, 3, 4], 2, {configurable: false});
}

assertEq(testLenientAndStrict('var a = arr(); a.length = 2; a',
                              returnsCopyOf([1, 2, 3]),
                              raisesException(TypeError)),
         true);

// Internally, SpiderMonkey has two representations for arrays:
// fast-but-inflexible, and slow-but-flexible. Adding a non-index property
// to an array turns it into the latter. We should test on both kinds.
function addx(obj) {
  obj.x = 5;
  return obj;
}

assertEq(testLenientAndStrict('var a = addx(arr()); a.length = 2; a',
                              returnsCopyOf(addx([1, 2, 3])),
                              raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
