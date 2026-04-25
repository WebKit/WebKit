/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Deleting an identifier is a syntax error in strict mode code only. */
assertEq(testLenientAndStrict('delete x;',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/*
 * A reference expression surrounded by parens is itself a reference
 * expression.
 */
assertEq(testLenientAndStrict('delete (x);',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Deleting other sorts of expressions are not syntax errors in either mode. */
assertEq(testLenientAndStrict('delete x.y;',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

/* Functions should inherit the surrounding code's strictness. */
assertEq(testLenientAndStrict('function f() { delete x; }',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Local directives override the surrounding code's strictness. */
assertEq(testLenientAndStrict('function f() { "use strict"; delete x; }',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

reportCompare(true, true);

var successfullyParsed = true;
