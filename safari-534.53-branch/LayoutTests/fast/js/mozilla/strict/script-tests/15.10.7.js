/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(testLenientAndStrict('var r = /foo/; r.source = "bar"; r.source',
                              returns("foo"), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var r = /foo/; delete r.source',
                              returns(false), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var r = /foo/; r.global = true; r.global',
                              returns(false), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var r = /foo/; delete r.global',
                              returns(false), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var r = /foo/; r.ignoreCase = true; r.ignoreCase',
                              returns(false), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var r = /foo/; delete r.ignoreCase',
                              returns(false), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var r = /foo/; r.multiline = true; r.multiline',
                              returns(false), raisesException(TypeError)),
         true);
assertEq(testLenientAndStrict('var r = /foo/; delete r.multiline',
                              returns(false), raisesException(TypeError)),
         true);

assertEq(testLenientAndStrict('var r = /foo/; r.lastIndex = 42; r.lastIndex',
                              returns(42), returns(42)),
         true);
assertEq(testLenientAndStrict('var r = /foo/; delete r.lastIndex',
                              returns(false), raisesException(TypeError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
