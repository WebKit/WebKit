/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Octal integer literal at top level. */
assertEq(testLenientAndStrict('010',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Octal integer literal in strict function body */
assertEq(parseRaisesException(SyntaxError)
         ('function f() { "use strict"; 010; }'),
         true);
                              

/*
 * Octal integer literal after strict function body (restoration of
 * scanner state)
 */
assertEq(parsesSuccessfully('function f() { "use strict"; }; 010'),
         true);

/* Octal integer literal in function body */
assertEq(parsesSuccessfully('function f() { 010; }'),
         true);

reportCompare(true, true);

var successfullyParsed = true;
