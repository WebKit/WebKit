//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

var sf0 = SIMD.Bool32x4(1.35, -2.0, 3.4, 0.0);
var sf1 = SIMD.Bool32x4(1.35, -2.0, 3.4, 0.0);
var sf2 = SIMD.Bool32x4(1.35, -2.0, 3.14, 0.0);
var sf3 = SIMD.Bool32x4(0, Infinity, NaN, -0);

function testConstructor()
{
    ////////////////////
    //Constructor
    ///////////////////
    equal("object", typeof SIMD.Bool32x4.prototype);
    equal("function", typeof SIMD.Bool32x4.prototype.constructor);
    equal("function", typeof SIMD.Bool32x4.prototype.toString);
    equal("function", typeof SIMD.Bool32x4.prototype.toLocaleString);
    equal("function", typeof SIMD.Bool32x4.prototype.valueOf);
    equal(SIMD.Bool32x4, SIMD.Bool32x4.prototype.constructor);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Bool32x4, 'prototype');
    equal(false, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(false, descriptor.configurable); 
}

function testToStringTag()
{
    ///////////////////
    //SIMDConstructor.prototype [ @@toStringTag ]
    ///////////////////
    var sym = Symbol('sym');
    var symbol_object = Object(sym);
    equal("SIMD.Bool32x4", SIMD.Bool32x4.prototype[Symbol.toStringTag]);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Bool32x4.prototype, Symbol.toStringTag);
    equal(false, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);
}

function testToPrimitive()
{
    ///////////////////
    //SIMDConstructor.prototype [ @@toPrimitive ] 
    ///////////////////
    equal("function",typeof SIMD.Bool32x4.prototype[Symbol.toPrimitive]);
    equal(1, SIMD.Bool32x4.prototype[Symbol.toPrimitive].length);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Bool32x4.prototype, Symbol.toPrimitive);
    equal(false, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);

    var functionToString = SIMD.Bool32x4.prototype[Symbol.toPrimitive].toString();
    var actualName = functionToString.substring(9, functionToString.indexOf('('));
    equal("[Symbol.toPrimitive]",actualName);

    ///////////////////////////
    //Overriding @@ToPrimitive
    ///////////////////////////
    SIMD.Bool32x4.prototype[Symbol.toPrimitive] = undefined;
    var G;
    var functionBackup = SIMD.Bool32x4.prototype.toLocaleString;
    SIMD.Bool32x4.prototype.toLocaleString = function(){
        G = this;
    }
    sf0.toLocaleString();
    try{var x =  G + G;}
    catch(e)
    {equal("Error: Number expected", e);}  //@@toPrimitive is not writable

    //@@toPrimitive is configurable. 
    memberBackup = Object.getOwnPropertyDescriptor(SIMD.Bool32x4.prototype, Symbol.toPrimitive);
    delete SIMD.Bool32x4.prototype[Symbol.toPrimitive];
    SIMD.Bool32x4.prototype[Symbol.toPrimitive] = function() { return 1;}
    equal(2, G + G);
    equal(0, G - G);
    equal(1, G * G);
    equal(1, G / G);
    equal(true, G == G);
    equal(true, G === G);
    equal(false, G > G);
    equal(true, G >= G);
    equal(false, G < G);
    equal(true, G >= G);

    delete SIMD.Bool32x4.prototype[Symbol.toPrimitive];
    SIMD.Bool32x4.prototype[Symbol.toPrimitive] = function() { return undefined;}
    equal(NaN, G + G);
    equal(NaN, G - G);
    equal(NaN, G * G);
    equal(NaN, G / G);
    equal(true, G == G);
    equal(true, G === G);
    equal(false, G > G);
    equal(false, G >= G);
    equal(false, G < G);
    equal(false, G >= G);

    delete SIMD.Bool32x4.prototype[Symbol.toPrimitive];
    SIMD.Bool32x4.prototype[Symbol.toPrimitive] = function() { return "test";}
    equal("testtest", G + G);
    equal(NaN, G - G);
    equal(NaN, G * G);
    equal(NaN, G / G);
    equal(true, G == G);
    equal(true, G === G);
    equal(false, G > G);
    equal(true, G >= G);
    equal(false, G < G);
    equal(true, G >= G);

    delete SIMD.Bool32x4.prototype[Symbol.toPrimitive];
    SIMD.Bool32x4.prototype[Symbol.toPrimitive] = 5;
    try{var x =  G + G;}
    catch(e)
    {equal("TypeError: The value of the property 'Symbol.toPrimitive' is not a Function object", e);}

    SIMD.Bool32x4.prototype[Symbol.toPrimitive] = memberBackup;
    SIMD.Bool32x4.prototype.toLocaleString = functionBackup
}

function testValueOf()
{
    ///////////////////
    //ValueOf
    ///////////////////
    descriptor = Object.getOwnPropertyDescriptor(SIMD.Bool32x4.prototype, 'valueOf');
    equal(true, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);
    equal("bool32x4", typeof sf0.valueOf());
    equal("SIMD.Bool32x4(true, true, true, false)", sf0.toString());
}

function testToString_LocaleString()
{
    ///////////////////
    //ToString / ToLocaleString
    ///////////////////
    descriptor = Object.getOwnPropertyDescriptor(SIMD.Bool32x4.prototype, 'toString');
    equal(true, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Bool32x4.prototype, 'toLocaleString');
    equal(true, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);

    equal("SIMD.Bool32x4(true, true, true, false)", sf0.toString()); //If float use ToString on lanes, the right values will be available.
    equal("SIMD.Bool32x4(true, true, true, false)", sf0.toLocaleString());
    
    equal("SIMD.Bool32x4(false, true, false, false)", sf3.toString()); 
    // equal("SIMD.Bool32x4(0, ?, NaN, 0)", sf3.toLocaleString());   //How should this work?
    var origFn = SIMD.Bool32x4.prototype.toLocaleString;
    var G5;
    SIMD.Bool32x4.prototype.toLocaleString = function() {
        G5 = this;
        return "NEWTOSTRING";
    }

    equal("undefined", typeof(G5));
    equal("NEWTOSTRING", sf0.toLocaleString());
    equal("object", typeof(G5));
    equal("NEWTOSTRING", G5.toLocaleString());
    equal("NEWTOSTRING", sf0.toLocaleString());
     SIMD.Bool32x4.prototype.toLocaleString = origFn;

    ///////////////////
    //PrototypeLinking toString
    ///////////////////
    var origFn = SIMD.Bool32x4.prototype.toString;
    SIMD.Bool32x4.prototype.toString = function() {
        G = this;
        return "NEWTOSTRING";
    }

    equal("undefined", typeof(G));
    equal("NEWTOSTRING", sf0.toString());
    equal("object", typeof(G));
    equal("NEWTOSTRING", G.toString());
    equal("NEWTOSTRING", sf0.toString());
    SIMD.Bool32x4.prototype.toString = origFn;
}

function testProtoLinking()
{
    ///////////////////
    //Inheritance 
    ///////////////////
    SIMD.Bool32x4.prototype.myToString = function() {
        return "MYTOSTRING";
    }
    equal("MYTOSTRING", sf0.myToString());
    equal("MYTOSTRING", G.myToString());

    ///////////////////
    //PrototypeLinking valueOf
    ///////////////////
    SIMD.Bool32x4.prototype.valueOf = function() {
        G1 = this;
        return  "NEWVALUEOF";
    }

    var G1;
    equal("undefined", typeof(G1));
    equal("NEWVALUEOF", sf0.valueOf());
    equal("object", typeof(G1));
    equal("NEWVALUEOF", G1.valueOf());
    equal("NEWVALUEOF", sf0.valueOf());
}

function testValueSemantics() 
{
    ///////////////////
    //Value semantics
    ///////////////////
    equal(true, (sf0 == sf0));
    equal(true, (sf0 === sf0));
    equal(true, (sf0 == sf1));
    equal(true, (sf0 === sf1));
    equal(true, (sf0 == sf2));
    equal(true, (sf0 === sf2));

    ///////////////////
    //Comparision
    //////////////////
    try{var x = sf0 > sf1;}
    catch(e)
    {equal("TypeError: SIMD type: cannot be converted to a number", e);}
    
    try{var x = sf0 < sf1;}
    catch(e)
    {equal("TypeError: SIMD type: cannot be converted to a number", e);}

    try{var x = sf0 >= sf1;}
    catch(e)
    {equal("TypeError: SIMD type: cannot be converted to a number", e);}

    try{var x = sf0 <= sf1;}
    catch(e)
    {equal("TypeError: SIMD type: cannot be converted to a number", e);}

    ///////////////////////
    //Arithmetic operators
    ///////////////////////

    equal("SIMD.Bool32x4(false, true, false, false)test", sf3 + "test");
    equal("testSIMD.Bool32x4(false, true, false, false)", "test" + sf3);

    try{var x =  sf3 + sf3;}
    catch(e)
    {equal("Error: Number expected", e);} //Check
    
    try{var x =  sf3 - sf3;}
    catch(e)
    {equal("Error: Number expected", e);} 
    
    try{var x =  sf3 / sf3;}
    catch(e)
    {equal("Error: Number expected", e);} 
    
    try{var x =  sf3 * sf3;}
    catch(e)
    {equal("Error: Number expected", e);}
}

//The following test can be enabled when extract lane builtin for bools are merged. 
// function testHorizontalOR()
// {
//     var b1 = SIMD.Bool32x4(true, false, false, true);
//
//     SIMD.Bool32x4.prototype.horizontalOR = function()
//     {
//         var b4 = this.valueOf();
//         return (SIMD.Bool32x4.extractLane(b4, 0) ||
//                 SIMD.Bool32x4.extractLane(b4, 1) ||
//                 SIMD.Bool32x4.extractLane(b4, 2) ||
//                 SIMD.Bool32x4.extractLane(b4, 3) );
//      }
//
//      equal(true, b1.horizontalOR());
// }

function testWrapperObject() 
{

    testConstructor();
    testToStringTag();
    testToPrimitive();
    testValueOf();
    testToString_LocaleString();
    testProtoLinking();
    testValueSemantics();
    // testHorizontalOR()
}

testWrapperObject();
print("PASS");
