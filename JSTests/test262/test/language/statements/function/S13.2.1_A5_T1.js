// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Closures are admitted
es5id: 13.2.1_A5_T1
description: Sorting with closure
---*/

var __arr = [4,3,2,1,4,3,2,1,4,3,2,1];
//Sort uses closure
//
__arr.sort(
	function(x,y) { 
		if (x>y){return -1;} 
		if (x<y){return 1;} 
		if (x==y){return 0;} 
	}
);

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__arr.toString() !== [4,4,4,3,3,3,2,2,2,1,1,1].toString()) {
	throw new Test262Error('#1: __arr.toString() === [4,4,4,3,3,3,2,2,2,1,1,1].toString(). Actual: __arr.toString() ==='+__arr.toString());
}

//
//////////////////////////////////////////////////////////////////////////////
