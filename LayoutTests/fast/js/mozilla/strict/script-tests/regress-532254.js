/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(testLenientAndStrict('function f(eval,[x]){}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
