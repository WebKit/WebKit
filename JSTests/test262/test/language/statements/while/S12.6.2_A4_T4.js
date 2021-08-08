// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    "break" within a "while" Statement is allowed and performed as described
    in 12.8
es5id: 12.6.2_A4_T4
description: "\"break\" and VariableDeclaration within a \"while\" Statement"
---*/

do_out : while(1===1) {
    if(__in__do__before__break)break;
    var __in__do__before__break="can't";
    do_in : while (1) {
        var __in__do__IN__before__break="get";
        break;
        var __in__do__IN__after__break="no";
    } ;
    var __in__do__after__break="Satisfaction";
} ;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (!(__in__do__before__break&&__in__do__IN__before__break&&!__in__do__IN__after__break&&__in__do__after__break)) {
	throw new Test262Error('#1: Break inside do-while is allowed as its described at standard');
}
//
//////////////////////////////////////////////////////////////////////////////
