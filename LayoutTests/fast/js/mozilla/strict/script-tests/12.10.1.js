/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * 'with' statements are forbidden in strict top-level code. This is
 * eval code, but that's close enough.
 */
assertEq(testLenientAndStrict('with (1) {}',
                              completesNormally,
                              raisesException(SyntaxError)),
         true);

/* 'with' statements are forbidden in strict function code. */
assertEq(testLenientAndStrict('function f() { "use strict"; with (1) {} }',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
                              
/*
 * A use strict directive in a function mustn't affect the strictness
 * of subsequent code.
 */
assertEq(parsesSuccessfully('function f() { "use strict"; }; with (1) {}'),
         true);

reportCompare(true, true);

var successfullyParsed = true;
