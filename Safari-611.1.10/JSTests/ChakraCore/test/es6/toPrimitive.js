//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
       name: "Number Object Test",
       body: function ()
       {
            var a = new Number(1);
            a[Symbol.toPrimitive] = function(hint)
            {
                if(hint == "string")
                {
                    return "a";
                }
                else
                {
                    return 10;
                }
            }

            assert.isTrue(10 == a,"should now call @@toprimitive and return 10");
            assert.isTrue(11 == a+1,"should now call @@toprimitive with default hint and return 10 then add 1 = 11");
            assert.areEqual("a",String(a),"should now call @@toPrimitive and return a");
        }
    },
    {
        name: "Symbol Tests",
        body: function ()
        {

            var o  = Object.getOwnPropertyDescriptor(Symbol,"toPrimitive");
            assert.isFalse(o.writable,     "Symbol @@toPrimitive is not writable");
            assert.isFalse(o.enumerable,   "Symbol @@toPrimitive is not enumerable");
            assert.isFalse(o.configurable, "Symbol @@toPrimitive is not configurable");


            assert.areEqual("[Symbol.toPrimitive]", Symbol.prototype[Symbol.toPrimitive].name, "string should be [Symbol.toPrimitive]");
            var o  = Object.getOwnPropertyDescriptor(Symbol.prototype,Symbol.toPrimitive);
            assert.isFalse(o.writable,     "Symbol @@toPrimitive is not writable");
            assert.isFalse(o.enumerable,   "Symbol @@toPrimitive is not enumerable");
            assert.isTrue(o.configurable,  "Symbol @@toPrimitive is configurable");
            assert.throws(function() { Symbol.prototype[Symbol.toPrimitive](); }, TypeError, "Symbol[@@toPrimitive] throws no matter the parameters", "Symbol[Symbol.toPrimitive]: 'this' is not a Symbol object");

            var s = Symbol();
            assert.areEqual(s, Object(s)[Symbol.toPrimitive](), ""); // true
            assert.areEqual(s, Symbol.prototype[Symbol.toPrimitive].call(s), ""); // true

            assert.areEqual(Symbol.toPrimitive, Symbol.toPrimitive[Symbol.toPrimitive](), "Symbol.toPrimitive");
            assert.areEqual(Symbol.iterator, Symbol.iterator[Symbol.toPrimitive](), "Symbol.iterator");
            assert.areEqual(Symbol.hasInstance, Symbol.hasInstance[Symbol.toPrimitive](), "Symbol.hasInstance");
            assert.areEqual(Symbol.isConcatSpreadable, Symbol.isConcatSpreadable[Symbol.toPrimitive](), "Symbol.isConcatSpreadable");
            assert.areEqual(Symbol.match, Symbol.match[Symbol.toPrimitive](), "Symbol.match");
            assert.areEqual(Symbol.replace, Symbol.replace[Symbol.toPrimitive](), "Symbol.replace");
            assert.areEqual(Symbol.search, Symbol.search[Symbol.toPrimitive](), "Symbol.search");
            assert.areEqual(Symbol.split, Symbol.split[Symbol.toPrimitive](), "Symbol.split");
            assert.areEqual(Symbol.toStringTag, Symbol.toStringTag[Symbol.toPrimitive](), "Symbol.toStringTag");
            assert.areEqual(Symbol.unscopables, Symbol.unscopables[Symbol.toPrimitive](), "Symbol.unscopables");
        }
    },
    {
       name: "Date Test",
       body: function ()
       {
            assert.areEqual("[Symbol.toPrimitive]", Date.prototype[Symbol.toPrimitive].name, "string should be [Symbol.toPrimitive]");
            var o  = Object.getOwnPropertyDescriptor(Date.prototype,Symbol.toPrimitive);
            assert.isFalse(o.writable,     "Date @@toPrimitive is not writable");
            assert.isFalse(o.enumerable,   "Date @@toPrimitive is not enumerable");
            assert.isTrue(o.configurable,  "Date @@toPrimitive is configurable");

            var d = new Date(2014,5,30,8, 30,45,2);
            assert.areEqual(d.toString(),d[Symbol.toPrimitive]("string"), "check that the string hint toPrimitive returns toString");
            assert.areEqual(d.toString(),d[Symbol.toPrimitive]("default"), "check that default has the same behaviour as string hint");
            assert.areEqual(d.valueOf(),d[Symbol.toPrimitive]("number"),"check that the number hint toPrimitive returns valueOf");

            assert.throws(function() {d[Symbol.toPrimitive]("boolean")}, TypeError, "boolean is an invalid hint");
            assert.throws(function() {d[Symbol.toPrimitive]({})}, TypeError, "provided hint must be strings or they results in a type error");

            assert.areEqual(d.toString()+10,d+10,"addition provides no hint resulting default hint which is string");
            assert.areEqual(d.valueOf(), (new Number(d)).valueOf(),"conversion toNumber calls toPrimitive with hint number");

            delete Date.prototype[Symbol.toPrimitive];
            assert.isFalse(Date.prototype.hasOwnProperty(Symbol.toPrimitive),"Property is configurable, should be able to delete");
            assert.areEqual(d.toString()+10,d+10,"(fall back to OriginalToPrimitive) addition provides no hint resulting default hint which is string");
            assert.areEqual(d.valueOf(), (new Number(d)).valueOf(),"(fall back to OriginalToPrimitive) conversion toNumber calls toPrimitive with hint number");
            Object.defineProperty(Date.prototype, Symbol.toPrimitive, o); // restore deleted [@@toPrimitive] property

            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(undefined, "default")), TypeError, "this=undefined", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(null, "default")), TypeError, "this=null", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(true, "default")), TypeError, "this=true", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(false, "default")), TypeError, "this=false", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(0, "default")), TypeError, "this=0", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(NaN, "default")), TypeError, "this=NaN", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call('', "default")), TypeError, "this=''", "Date[Symbol.toPrimitive]: 'this' is not an Object");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call('abc', "default")), TypeError, "this='abc'", "Date[Symbol.toPrimitive]: 'this' is not an Object");

            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call()), TypeError, "this=undefined no hint", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(undefined)), TypeError, "this=undefined hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(null)), TypeError, "this=null hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(true)), TypeError, "this=true hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(false)), TypeError, "this=false hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(0)), TypeError, "this=0 hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(NaN)), TypeError, "this=NaN hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call('')), TypeError, "this='' hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call('abc')), TypeError, "this='abc' hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(Date.prototype[Symbol.toPrimitive].call(function(){})), TypeError, "this=function(){} hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");

            assert.throws(()=>(d[Symbol.toPrimitive]()), TypeError, "no hint", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](undefined)), TypeError, "hint=undefined", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](null)), TypeError, "hint=null", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](true)), TypeError, "hint=true", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](false)), TypeError, "hint=false", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](0)), TypeError, "hint=0", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](NaN)), TypeError, "hint=NaN", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive]('')), TypeError, "hint=''", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive]('abc')), TypeError, "hint='abc'", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive](function(){})), TypeError, "hint=function(){}", "Date[Symbol.toPrimitive]: invalid hint");
            assert.throws(()=>(d[Symbol.toPrimitive]({})), TypeError, "hint=={}", "Date[Symbol.toPrimitive]: invalid hint");
       }
    },
    {
       name: "Object toPrimitive Test",
       body: function ()
       {
            var o = { toString : function () {return "o"}, valueOf : function() { return 0;}};

            o[Symbol.toPrimitive] = function(hint)
            {
                if("string" == hint)
                {
                    return this.toString()+" (hint String)";
                }
                else if("number" == hint)
                {
                    return this.valueOf()+2;
                }
                else
                {
                    return " (hint default) ";
                }
            }
            assert.areEqual(" (hint default) ", o[Symbol.toPrimitive](), "Test to check the string is properly being returned");
            assert.areEqual("o (hint String)",  o[Symbol.toPrimitive]("string"), "Test to check the string is properly being returned");
            assert.areEqual(2, o[Symbol.toPrimitive]("number"), "Test to check the integer is properly being returned");


           assert.areEqual(2,(new Number(o)).valueOf(),"toNumber should call toPrimitive which should invoke the user defined behaviour for @@toPrimitive with hint number"); // conversion toNumber -> toPrimitive(hint number)
           assert.areEqual("1 (hint default) 1",1+o+1,"add should call toPrimitive which should invoke the user defined behaviour for @@toPrimitive with no hint"); // add -> toPrimitive(hint none)
           assert.areEqual("o (hint String)",(new String(o)).toString(),"toString should call toPrimitive which should invoke the user defined behaviour for @@toPrimitive with hint string"); // conversion toString -> toPrimitive(hint string)
       }
    },
    {
       name: "Object toPrimitive must be Function or null else Throws typeError",
       body: function ()
       {
            var o = { toString : function () {return "o"}, valueOf : function() { return 0;}};

            o[Symbol.toPrimitive]  = {}; // can only be a  function else type error
            assert.throws(function() {var a = o+1;}, TypeError, "o[Symbol.toPrimitive] must be a function", "The value of the property 'Symbol.toPrimitive' is not a Function object");

            o[Symbol.toPrimitive] = null;
            assert.doesNotThrow(function() {var a = o+1;}, "If o[Symbol.toPrimitive] is null it is ignored");
        }
    },
// In ScriptLanguageVersion6 the ActiveXObject constructor is removed and is unable to be used for this test. Disabling until different object type can be found
// that can be used instead.
//    {
//       name: "Object toPrimitive must return  ECMA Type else TypeError",
//       body: function ()
//       {
//            var o = { toString : function () {return "o"}, valueOf : function() { return 0;}};
//            var c = new ActiveXObject("Excel.Application");
//            o[Symbol.toPrimitive] = function(hint)
//            {
//                return c;
//            }
//            assert.throws(function() {var a = o+1;}, TypeError, "o[Symbol.toPrimitive] must return an ECMA language value");
//
//        }
//    },
    {
       name: "String Object Test",
       body: function ()
       {
            var a = new String("a");
            a[Symbol.toPrimitive] = function(hint)
            {
                if(hint == "string")
                {
                    return "var_a";
                }
                else
                {
                    return -1;
                }
            }
            assert.isTrue(-1 == a,"should now call @@toprimitive and return -1");
            assert.isTrue("var_a" == String(a),"should now call @@toprimitive and return var_a");
            assert.isTrue(0 == a+1,"should now call @@toprimitive and return -1+1 = 0");

            assert.isTrue("var_a1" == String(a)+1,"should now call @@toprimitive and return var_a1");
            assert.areEqual(-1,Number(a),"should now call @@toPrimitive and return a");
        }
    },
    {
        name: "ToPrimitive calling user-defined [@@toPrimitive] method",
        body: function ()
        {
            var o = {}, o1 = {}, o2 = {};

            var retVal, hint;

            o[Symbol.toPrimitive] = function(h) { hint.push(h); return retVal; }
            o1[Symbol.toPrimitive] = function(h) { hint.push("o1:"+h); return retVal; }
            o2[Symbol.toPrimitive] = function(h) { hint.push("o2:"+h); return retVal; }

            // Ensuring OrdinaryToPrimitive is not called
            Object.defineProperty(o, "toString", { writable:true, configurable:true, enumerable:true, value: function() {  throw Error("Unexpected toString() call on o"); } });
            Object.defineProperty(o1, "toString", { writable:true, configurable:true, enumerable:true, value: function() {  throw Error("Unexpected toString() call on o1"); } });
            Object.defineProperty(o2, "toString", { writable:true, configurable:true, enumerable:true, value: function() {  throw Error("Unexpected toString() call on o2"); } });
            Object.defineProperty(o, "valueOf", { writable:true, configurable:true, enumerable:true, value: function() {  throw Error("Unexpected valueOf() call on o"); } });
            Object.defineProperty(o1, "valueOf", { writable:true, configurable:true, enumerable:true, value: function() {  throw Error("Unexpected valueOf() call on o1"); } });
            Object.defineProperty(o2, "valueOf", { writable:true, configurable:true, enumerable:true, value: function() {  throw Error("Unexpected valueOf() call on o2"); } });

            var verifyToPrimitive = function(func, expectedResult, expectedHint, description) {
                hint = [];
                assert.areEqual(expectedResult, func(), "result:" + description);
                assert.areEqual(expectedHint, hint, "hint:" + description);
            }

            //
            // ToNumber calls ToPrimitive(input, 'number')
            //

            retVal = NaN;
            verifyToPrimitive(()=>Number(o), NaN, ["number"], "Number()");
            verifyToPrimitive(()=>new Uint8Array([o]), new Uint8Array([NaN]), ["number"], "TypedArray constructor");
            verifyToPrimitive(()=>isFinite(o), false, ["number"], "isFinite()");
            verifyToPrimitive(()=>isNaN(o), true, ["number"], "isNaN()");

            retVal = 100;
            verifyToPrimitive(()=>(1-o), -99, ["number"], "1-o");
            verifyToPrimitive(()=>(o-2), 98, ["number"], "o-2");
            verifyToPrimitive(()=>(1*o), 100, ["number"], "1*o");
            verifyToPrimitive(()=>(o*2), 200, ["number"], "o*2");
            verifyToPrimitive(()=>Math.log10(o), 2, ["number"], "Math.log10()");
            verifyToPrimitive(()=>(o1-o2), 0, ["o1:number", "o2:number"], "o1-o2");
            verifyToPrimitive(()=>(o2/o1), 1, ["o2:number", "o1:number"], "o2/o1");

            retVal = 100;
            var n = o;
            verifyToPrimitive(()=>(++n), 101, ["number"], "++n");
            n = o;
            verifyToPrimitive(()=>(n++), 100, ["number"], "n++");
            n = o;
            verifyToPrimitive(()=>(--n), 99, ["number"], "--n");
            n = o;
            verifyToPrimitive(()=>(n--), 100, ["number"], "n--");


            retVal = "abc";
            verifyToPrimitive(()=>(1-o), NaN, ["number"], "1-o ([@@toPrimitive] returns string)");
            verifyToPrimitive(()=>(o-2), NaN, ["number"], "o-2 ([@@toPrimitive] returns string)");

            //
            // ToString calls ToPrimitive(input, 'string')
            //

            retVal = NaN;
            verifyToPrimitive(()=>String(o), "NaN", ["string"], "String()");
            verifyToPrimitive(()=>parseFloat(o), NaN, ["string"], "parseFloat()");
            verifyToPrimitive(()=>parseInt(o), NaN, ["string"], "parseInt()");
            verifyToPrimitive(()=>decodeURI(o), "NaN", ["string"], "decodeURI()");

            //
            // ToPropertyKey calls ToPrimitive(input, 'string')
            //

            retVal = NaN;
            var x = {};
            verifyToPrimitive(()=>Object.defineProperty(x, o, {writable:true, enumerable:true, configurable:true, value: 'abc123'}), x, ["string"], "Object.defineProperty()");
            verifyToPrimitive(()=>x[o], 'abc123', ["string"], "x[o]");
            verifyToPrimitive(()=>x.hasOwnProperty(o), true, ["string"], "Object.prototype.hasOwnProperty()");
            verifyToPrimitive(()=>x.propertyIsEnumerable(o), true, ["string"], "Object.prototype.propertyIsEnumerable()");
            verifyToPrimitive(()=>(o in x), true, ["string"], "o in x");

            //
            // + operator calls ToPrimitive(input, 'default')
            //

            retVal = 100;
            verifyToPrimitive(()=>(o+1), 101, ["default"], "o+1");
            verifyToPrimitive(()=>(2+o), 102, ["default"], "2+o");
            verifyToPrimitive(()=>(o+'abc'), '100abc', ["default"], "o+'abc'");
            verifyToPrimitive(()=>('abc'+o), 'abc100', ["default"], "'abc'+o");
            verifyToPrimitive(()=>(o1+o2), 200, ["o1:default", "o2:default"], "o1+o2");
            verifyToPrimitive(()=>(o2+o1), 200, ["o2:default", "o1:default"], "o2+o1");

            retVal = "abc";
            verifyToPrimitive(()=>(o+1), "abc1", ["default"], "o+1 ([@@toPrimitive] returns string)");
            verifyToPrimitive(()=>(2+o), "2abc", ["default"], "2+1 ([@@toPrimitive] returns string)");
            verifyToPrimitive(()=>(o+'def'), 'abcdef', ["default"], "o+'def'");
            verifyToPrimitive(()=>('def'+o), 'defabc', ["default"], "'def'+o");
            verifyToPrimitive(()=>(o1+o2), "abcabc", ["o1:default", "o2:default"], "o1+o2");
            verifyToPrimitive(()=>(o2+o1), "abcabc", ["o2:default", "o1:default"], "o2+o1");

            //
            // abstract relational comparison calls ToPrimitive(input, "number")
            //

            retVal = 100;
            verifyToPrimitive(()=>(o<1), false, ["number"], "o<1");
            verifyToPrimitive(()=>(1<o), true, ["number"], "1<o");
            verifyToPrimitive(()=>(o<=25), false, ["number"], "o<=25");
            verifyToPrimitive(()=>(-9<=o), true, ["number"], "-9<=o");
            verifyToPrimitive(()=>(o>1), true, ["number"], "o>1");
            verifyToPrimitive(()=>(1>o), false, ["number"], "1>o");
            verifyToPrimitive(()=>(o>=25), true, ["number"], "o>=25");
            verifyToPrimitive(()=>(-9>=o), false, ["number"], "-9>=o");
            verifyToPrimitive(()=>(o1<o2), false, ["o1:number", "o2:number"], "o1<o2");
            verifyToPrimitive(()=>(o2<=o1), true, ["o2:number", "o1:number"], "o2<=o1");
            verifyToPrimitive(()=>(o1>o2), false, ["o1:number", "o2:number"], "o1>o2");
            verifyToPrimitive(()=>(o2>=o1), true, ["o2:number", "o1:number"], "o2>=o1");

            //
            // abstract equality comparison calls ToPrimitive(input, "default")
            //

            verifyToPrimitive(()=>(o1==o2), false, [], ""); // 1. If Type(x) is the same of Type(y) return Strict Equality Comparison x === y

            retVal = 100;
            verifyToPrimitive(()=>(o==100), true, ["default"], "o==100");
            verifyToPrimitive(()=>(100==o), true, ["default"], "100==o");
            verifyToPrimitive(()=>(o==1), false, ["default"], "o==1");
            verifyToPrimitive(()=>(1==o), false, ["default"], "1==o");

            retVal = true;
            verifyToPrimitive(()=>(o==true), true, ["default"], "o==true");
            verifyToPrimitive(()=>(true==o), true, ["default"], "true==o");
            verifyToPrimitive(()=>(o==false), false, ["default"], "o==false");
            verifyToPrimitive(()=>(false==o), false, ["default"], "false==o");

            retVal = 'abc';
            verifyToPrimitive(()=>(o=='abc'), true, ["default"], "o=='abc'");
            verifyToPrimitive(()=>('abc'==o), true, ["default"], "'abc'==o");
            verifyToPrimitive(()=>(o=='abc1'), false, ["default"], "o=='abc1'");
            verifyToPrimitive(()=>('abc1'==o), false, ["default"], "'abc1'==o");

            retVal = Symbol();
            verifyToPrimitive(()=>(o==retVal), true, ["default"], "o==retVal (retVal=Symbol())");
            verifyToPrimitive(()=>(retVal==o), true, ["default"], "retVal==o (retVal=Symbol())");
            verifyToPrimitive(()=>(o==Symbol()), false, ["default"], "o==Symbol()");
            verifyToPrimitive(()=>(Symbol()==o), false, ["default"], "Symbol()==o");

            //
            // Date constructor calls ToPrimitive(input, "default")
            //

            retVal = 'Jan 1, 2016';
            verifyToPrimitive(()=>new Date(o).valueOf(), new Date(retVal).valueOf(), ["default"], "Date() constructor");

            //
            // Date.prototype.toJSON calls ToPrimitive(input, "number")
            //

            retVal = NaN;
            verifyToPrimitive(()=>Date.prototype.toJSON.call(o), null, ["number"], "Date.prototype.toJSON()");

            //
            // Date.prototype[@@toPrimitive] calls ToPrimitive
            //

            Object.defineProperty(o, 'toString', { writable:true, configurable:true, enumerable:true, value: function() { return 'abc'; } });
            Object.defineProperty(o, 'valueOf', { writable:true, configurable:true, enumerable:true, value: function() { return 123; } });
            assert.areEqual(123, Date.prototype[Symbol.toPrimitive].call(o, 'number'), "Date.prototype[@@toPrimitive].call(o, 'number')");
            assert.areEqual('abc', Date.prototype[Symbol.toPrimitive].call(o, 'string'), "Date.prototype[@@toPrimitive].call(o, 'string')");
            assert.areEqual('abc', Date.prototype[Symbol.toPrimitive].call(o, 'default'), "Date.prototype[@@toPrimitive].call(o, 'default')");

        }
    },
    {
        name: "ToPrimitive calling user-defined [@@toPrimitive] method that returns Object throws TypeError",
        body: function ()
        {
            var o = {};

            [{}, new Date(), Error(), new String(), new Boolean(), new Number()].forEach(function(retVal) {
                o[Symbol.toPrimitive] = function(h) { return retVal; }

                //
                // ToNumber
                //

                assert.throws(()=>(Number(o)), TypeError, "Number()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>new Uint8Array([o]), TypeError, "TypedArray constructor", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>isFinite(o), TypeError, "isFinite()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>isNaN(o), TypeError, "isNaN()", "[Symbol.toPrimitive]: invalid argument");

                assert.throws(()=>(1-o), TypeError, "1-o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o-2), TypeError, "o-2", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(1*o), TypeError, "1*o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o*2), TypeError, "o*2", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>Math.log10(o), TypeError, "Math.log10()", "[Symbol.toPrimitive]: invalid argument");

                var n = o;
                assert.throws(()=>(++n), TypeError, "++n", "[Symbol.toPrimitive]: invalid argument");
                n = o;
                assert.throws(()=>(n++), TypeError, "n++", "[Symbol.toPrimitive]: invalid argument");
                n = o;
                assert.throws(()=>(--n), TypeError, "--n", "[Symbol.toPrimitive]: invalid argument");
                n = o;
                assert.throws(()=>(n--), TypeError, "n--", "[Symbol.toPrimitive]: invalid argument");

                //
                // ToString
                //

                assert.throws(()=>String(o), TypeError, "String()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>parseFloat(o), TypeError, "parseFloat()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>parseInt(o), TypeError, "parseInt()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>decodeURI(o), TypeError, "decodeURI()", "[Symbol.toPrimitive]: invalid argument");

                //
                // ToPropertyKey
                //

                var x = {};
                assert.throws(()=>Object.defineProperty(x, o, {}), TypeError, "Object.defineProperty()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>x[o], TypeError, "x[o]", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>x.hasOwnProperty(o), TypeError, "Object.prototype.hasOwnProperty()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>x.propertyIsEnumerable(o), TypeError, "Object.prototype.propertyIsEnumerable()", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o in x), TypeError, "o in x", "[Symbol.toPrimitive]: invalid argument");

                //
                // + operator
                //

                assert.throws(()=>(o+1), TypeError, "o+1", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(2+o), TypeError, "2+o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o+'abc'), TypeError, "o+'abc'", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>('abc'+o), TypeError, "'abc'+o", "[Symbol.toPrimitive]: invalid argument");

                //
                // abstract relational comparison
                //

                assert.throws(()=>(o<1), TypeError, "o<1", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(1<o), TypeError, "1<o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o<=25), TypeError, "o<=25", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(-9<=o), TypeError, "-9<=o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o>1), TypeError, "o>1", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o>=25), TypeError, "o>=25", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(-9>=o), TypeError, "-9>=o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o<o), TypeError, "o<o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o<=o), TypeError, "o<=o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o>o), TypeError, "o>o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o>=o), TypeError, "o>=o", "[Symbol.toPrimitive]: invalid argument");

                //
                // abstract equality comparison
                //

                assert.isTrue(o==o, "o==o should not call ToPrimitive");
                assert.throws(()=>('abc'==o), TypeError, "'abc'==o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o=='abc'), TypeError, "o=='abc'", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(100==o), TypeError, "100==o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o==100), TypeError, "o==100", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(Symbol()==o), TypeError, "Symbol()==o", "[Symbol.toPrimitive]: invalid argument");
                assert.throws(()=>(o==Symbol()), TypeError, "o==Symbol()", "[Symbol.toPrimitive]: invalid argument");
            });
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
