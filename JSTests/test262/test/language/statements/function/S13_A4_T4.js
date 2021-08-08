// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production FunctionDeclaration: "function Identifier (
    FormalParameterList_opt ) { FunctionBody }" is processed by function
    declarations
es5id: 13_A4_T4
description: >
    Declaring a function that uses strings concatenaion opeator within
    its "return" Expression
---*/

function __func(){return arguments[0].name + " " + arguments[0].surname;};

//////////////////////////////////////////////////////////////////////////////
//CHECK#1 
if (typeof __func !== "function") {
	throw new Test262Error('#1: typeof __func === "function". Actual: typeof __func ==='+typeof __func);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (__func({name:'fox', surname:'malder'}) !== "fox malder") {
	throw new Test262Error('#2: __func({name:\'fox\', surname:\'malder\'}) === "fox malder". Actual: __func({name:\'fox\', surname:\'malder\'}) ==='+__func({name:'fox', surname:'malder'}));
}
//
//////////////////////////////////////////////////////////////////////////////

function func__(arg){return arg.name + " " + arg.surname;};

//////////////////////////////////////////////////////////////////////////////
//CHECK#3 
if (typeof func__ !== "function") {
	throw new Test262Error('#3: typeof func__ === "function". Actual: typeof __func ==='+typeof __func);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#4
if (func__({name:'john', surname:'lennon'}) !== "john lennon") {
	throw new Test262Error('#4: func__({name:\'john\', surname:\'lennon\'}) === "john lennon". Actual: __func({name:\'john\', surname:\'lennon\'}) ==='+__func({name:'john', surname:'lennon'}));
}
//
//////////////////////////////////////////////////////////////////////////////
