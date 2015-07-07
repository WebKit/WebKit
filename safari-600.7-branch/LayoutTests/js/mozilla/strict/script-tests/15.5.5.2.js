/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(testLenientAndStrict('"foo"[0] = 1',
                              returns(1), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('delete "foo"[0]',
                              returns(false), raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
