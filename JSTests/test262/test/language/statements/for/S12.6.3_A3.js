// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    While evaluating "for (ExpressionNoIn; FirstExpression; SecondExpression)
    Statement", ExpressionNoIn is evaulated first, FirstExpressoin is
    evaluated second
es5id: 12.6.3_A3
description: Using "(function(){throw "FirstExpression"})()" as FirstExpression
---*/

var __in__NotInExpression__, __in__NotInExpression__2, __in__for;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
try {
	for((function(){__in__NotInExpression__ = "checked";__in__NotInExpression__2 = "passed";})(); (function(){throw "FirstExpression"})(); (function(){throw "SecondExpression"})()) {
		__in__for="reached";
	}
	throw new Test262Error('#1: (function(){throw "SecondExpression"} lead to throwing exception');
} catch (e) {
	if (e !== "FirstExpression") {
		throw new Test262Error('#1: When for (ExpressionNoIn ; FirstExpression ; SecondExpression) Statement is evaluated first evaluates ExpressionNoIn then FirstExpression');
	}
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if ((__in__NotInExpression__ !== "checked")&(__in__NotInExpression__2!=="passed")) {
	throw new Test262Error('#2: (__in__NotInExpression__ === "checked")&(__in__NotInExpression__2==="passed")');
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#3
if (typeof __in__for !== "undefined") {
	throw new Test262Error('#3: typeof __in__for === "undefined". Actual:  typeof __in__for ==='+ typeof __in__for  );
}
//
//////////////////////////////////////////////////////////////////////////////
