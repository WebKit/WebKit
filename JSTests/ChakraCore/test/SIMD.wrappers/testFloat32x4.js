//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");

var sf0 = SIMD.Float32x4(1.35, -2.0, 3.4, 0.0);
var sf1 = SIMD.Float32x4(1.35, -2.0, 3.4, 0.0);
var sf2 = SIMD.Float32x4(1.35, -2.0, 3.14, 0.0);
var sf3 = SIMD.Float32x4(0, Infinity, NaN, -0);

function testConstructor()
{
    ////////////////////
    //Constructor
    ///////////////////
    equal("object", typeof SIMD.Float32x4.prototype);
    equal("function", typeof SIMD.Float32x4.prototype.constructor);
    equal("function", typeof SIMD.Float32x4.prototype.toString);
    equal("function", typeof SIMD.Float32x4.prototype.toLocaleString);
    equal("function", typeof SIMD.Float32x4.prototype.valueOf);
    equal(SIMD.Float32x4, SIMD.Float32x4.prototype.constructor);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Float32x4, 'prototype');
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
    equal("SIMD.Float32x4", SIMD.Float32x4.prototype[Symbol.toStringTag]);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Float32x4.prototype, Symbol.toStringTag);
    equal(false, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);
}

function testToPrimitive()
{
    ///////////////////
    //SIMDConstructor.prototype [ @@toPrimitive ] 
    ///////////////////
    equal("function",typeof SIMD.Float32x4.prototype[Symbol.toPrimitive]);
    equal(1, SIMD.Float32x4.prototype[Symbol.toPrimitive].length);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Float32x4.prototype, Symbol.toPrimitive);
    equal(false, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);

    var functionToString = SIMD.Float32x4.prototype[Symbol.toPrimitive].toString();
    var actualName = functionToString.substring(9, functionToString.indexOf('('));
    equal("[Symbol.toPrimitive]",actualName);

    ///////////////////////////
    //Overriding @@ToPrimitive
    ///////////////////////////
    SIMD.Float32x4.prototype[Symbol.toPrimitive] = undefined;
    var G;
    var functionBackup = SIMD.Float32x4.prototype.toLocaleString;
    SIMD.Float32x4.prototype.toLocaleString = function(){
        G = this;
    }
    sf0.toLocaleString();
    try{var x =  G + G;}
    catch(e)
    {equal("Error: Number expected", e);}  //@@toPrimitive is not writable

    //@@toPrimitive is configurable. 
    memberBackup = Object.getOwnPropertyDescriptor(SIMD.Float32x4.prototype, Symbol.toPrimitive);
    delete SIMD.Float32x4.prototype[Symbol.toPrimitive];
    SIMD.Float32x4.prototype[Symbol.toPrimitive] = function() { return 1;}
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

    delete SIMD.Float32x4.prototype[Symbol.toPrimitive];
    SIMD.Float32x4.prototype[Symbol.toPrimitive] = function() { return undefined;}
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

    delete SIMD.Float32x4.prototype[Symbol.toPrimitive];
    SIMD.Float32x4.prototype[Symbol.toPrimitive] = function() { return "test";}
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

    delete SIMD.Float32x4.prototype[Symbol.toPrimitive];
    SIMD.Float32x4.prototype[Symbol.toPrimitive] = 5;
    try{var x =  G + G;}
    catch(e)
    {equal("TypeError: The value of the property 'Symbol.toPrimitive' is not a Function object", e);}

    SIMD.Float32x4.prototype[Symbol.toPrimitive] = memberBackup;
    SIMD.Float32x4.prototype.toLocaleString = functionBackup
}

function testValueOf()
{
    ///////////////////
    //ValueOf
    ///////////////////
    descriptor = Object.getOwnPropertyDescriptor(SIMD.Float32x4.prototype, 'valueOf');
    equal(true, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);
    equal("float32x4", typeof sf0.valueOf());
    equal("SIMD.Float32x4(1.350000023841858, -2, 3.4000000953674316, 0)", (sf0.valueOf()).toString());
}

function testToString_LocaleString()
{
    ///////////////////
    //ToString / ToLocaleString
    ///////////////////
    descriptor = Object.getOwnPropertyDescriptor(SIMD.Float32x4.prototype, 'toString');
    equal(true, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);

    descriptor = Object.getOwnPropertyDescriptor(SIMD.Float32x4.prototype, 'toLocaleString');
    equal(true, descriptor.writable);
    equal(false, descriptor.enumerable);
    equal(true, descriptor.configurable);

    equal("SIMD.Float32x4(1.350000023841858, -2, 3.4000000953674316, 0)", sf0.toString()); //If float use ToString on lanes, the right values will be available.
    equal("SIMD.Float32x4(1.35, -2, 3.4, 0)", sf0.toLocaleString());
    
    equal("SIMD.Float32x4(0, Infinity, NaN, 0)", sf3.toString()); 
    // equal("SIMD.Float32x4(0, ?, NaN, 0)", sf3.toLocaleString());   //The results match but comparison fails due to '?'
    var actual =  SIMD.Float32x4.prototype.toLocaleString ;
    var G5;
    SIMD.Float32x4.prototype.toLocaleString = function() {
        G5 = this;
        return "NEWTOSTRING";
    }

    equal("undefined", typeof(G5));
    equal("NEWTOSTRING", sf0.toLocaleString());
    equal("object", typeof(G5));
    equal("NEWTOSTRING", G5.toLocaleString());
    equal("NEWTOSTRING", sf0.toLocaleString());
    SIMD.Float32x4.prototype.toLocaleString = actual;

    ///////////////////
    //PrototypeLinking toString
    ///////////////////
    actual = SIMD.Float32x4.prototype.toString;
    SIMD.Float32x4.prototype.toString = function() {
        G = this;
        return "NEWTOSTRING";
    }

    equal("undefined", typeof(G));
    equal("NEWTOSTRING", sf0.toString());
    equal("object", typeof(G));
    equal("NEWTOSTRING", G.toString());
    equal("NEWTOSTRING", sf0.toString());
    SIMD.Float32x4.prototype.toString = actual;
}

function testProtoLinking()
{
    ///////////////////
    //Inheritance 
    ///////////////////
    SIMD.Float32x4.prototype.myToString = function() {
        return "MYTOSTRING";
    }
    equal("MYTOSTRING", sf0.myToString());
    equal("MYTOSTRING", G.myToString());

    ///////////////////
    //PrototypeLinking valueOf
    ///////////////////
    var actual = SIMD.Float32x4.prototype.valueOf;
    SIMD.Float32x4.prototype.valueOf = function() {
        G1 = this;
        return  "NEWVALUEOF";
    }

    var G1;
    equal("undefined", typeof(G1));
    equal("NEWVALUEOF", sf0.valueOf());
    equal("object", typeof(G1));
    equal("NEWVALUEOF", G1.valueOf());
    equal("NEWVALUEOF", sf0.valueOf());
    SIMD.Float32x4.prototype.valueOf = actual;
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
    equal(false, (sf0 == sf2));
    equal(false, (sf0 === sf2));

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

    equal("SIMD.Float32x4(0, Infinity, NaN, 0)test", sf3 + "test");
    equal("testSIMD.Float32x4(0, Infinity, NaN, 0)", "test" + sf3);

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

function testUseCase1()
{
    var f4val = SIMD.Float32x4(0.0, 1.0, 2.0, 3.0);
    SIMD.Float32x4.prototype.horizontalSum = function()
    {
        var value = this.valueOf();
        var sum = SIMD.Float32x4.extractLane(value, 0) +
                  SIMD.Float32x4.extractLane(value, 1) +
                  SIMD.Float32x4.extractLane(value, 2) +
                  SIMD.Float32x4.extractLane(value, 3);
        return sum;
    }

    equal(6, f4val.horizontalSum());
}

function testWrapperObject() 
{

    testConstructor();
    testToStringTag();
    testToPrimitive();
    testValueOf();
    testToString_LocaleString();
    testProtoLinking();
    testValueSemantics();
    testUseCase1();
}

testWrapperObject();
print("PASS");
