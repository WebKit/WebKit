/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * In strict mode, it is a syntax error for an identifier to appear
 * more than once in a function's argument list.
 */

/*
 * The parameters of ordinary function definitions should not contain
 * duplicate identifiers.
 */
assertEq(testLenientAndStrict('function f(x,y) {}',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);
assertEq(testLenientAndStrict('function f(x,x) {}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f(x,y,z,y) {}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Exercise the hashed local name map case. */
assertEq(testLenientAndStrict('function f(a,b,c,d,e,f,g,h,d) {}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/*
 * SpiderMonkey has always treated duplicates in destructuring
 * patterns as an error. Strict mode should not affect this.
 */
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f([x,y]) {}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f([x,x]){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f(x,[x]){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * Strict rules apply to the parameters if the function's body is
 * strict.
 */
assertEq(testLenientAndStrict('function f(x,x) { "use strict" };',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * Calls to the function constructor should not be affected by the
 * strictness of the calling code, but should be affected by the
 * strictness of the function body.
 */
assertEq(testLenientAndStrict('Function("x","x","")',
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict('Function("x","y","")',
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict('Function("x","x","\'use strict\'")',
                              raisesException(SyntaxError),
                              raisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('Function("x","y","\'use strict\'")',
                              completesNormally,
                              completesNormally),
         true);


/*
 * The parameter lists of function expressions should not contain
 * duplicate identifiers.
 */
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function (x,x) 2)',
                               parseRaisesException(SyntaxError),
                               parseRaisesException(SyntaxError)),
         true);
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function (x,y) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * All permutations of:
 * - For the two magic identifiers 'arguments' or 'eval'
 *   - For function definitions, function expressions, expression closures,
 *     and getter and setter property definitions,
 *     - For forms that inherit their context's strictness, and forms that
 *       include their own strictness directives,
 *       - For ordinary parameters, array destructuring parameters, and 
 *         object destructuring parameters,
 *         - the magic identifiers may be used to name such parameters
 *           in lenient code, but not in strict code
 *       - the magic identifiers may be used as function names in lenient code,
 *         but not in strict code
 */
assertEq(testLenientAndStrict('function f(eval){}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f([eval]){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f({x:eval}){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function eval(){}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f(eval){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f([eval]){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f({x:eval}){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function eval(){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f(eval){})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f([eval]){})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f({x:eval}){})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function eval(){})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f(eval){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f([eval]){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f({x:eval}){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function eval(){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f(eval) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f([eval]) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f({x:eval}) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function eval() 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x(eval){}})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('({set x([eval]){}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('({set x({x:eval}){}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x(eval){"use strict";}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x([eval]){"use strict";}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x({x:eval}){"use strict";}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f(arguments){}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f([arguments]){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f({x:arguments}){}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function arguments(){}',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f(arguments){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('function f([arguments]){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function f({x:arguments}){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('function arguments(){"use strict";}',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f(arguments){})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f([arguments]){})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f({x:arguments}){})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function arguments(){})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f(arguments){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f([arguments]){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function f({x:arguments}){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('(function arguments(){"use strict";})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f(arguments) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f([arguments]) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function f({x:arguments}) 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// This is not valid ES5 syntax.
assertEq(testLenientAndStrict('(function arguments() 2)',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x(arguments){}})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('({set x([arguments]){}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
// Destructuring is not valid ES5 syntax.
assertEq(testLenientAndStrict('({set x({x:arguments}){}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x(arguments){"use strict";}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x([arguments]){"use strict";}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('({set x({x:arguments}){"use strict";}})',
                              parseRaisesException(SyntaxError),
                              parseRaisesException(SyntaxError)),
         true);

/*
 * Functions produced using the Function constructor may not use
 * 'eval' or 'arguments' as a parameter name if their body is strict
 * mode code. The strictness of the calling code does not affect the
 * constraints applied to the parameters.
 */
assertEq(testLenientAndStrict('Function("eval","")',
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict('Function("eval","\'use strict\';")',
                              raisesException(SyntaxError),
                              raisesException(SyntaxError)),
         true);
assertEq(testLenientAndStrict('Function("arguments","")',
                              completesNormally,
                              completesNormally),
         true);
assertEq(testLenientAndStrict('Function("arguments","\'use strict\';")',
                              raisesException(SyntaxError),
                              raisesException(SyntaxError)),
         true);


reportCompare(true, true);

var successfullyParsed = true;
