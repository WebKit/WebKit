/* The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * 
 */
/**
    File Name:          15.4.4.2.js
    ECMA Section:       15.4.4.2 Array.prototype.toString()
    Description:        The elements of this object are converted to strings
                        and these strings are then concatenated, separated by
                        comma characters.  The result is the same as if the
                        built-in join method were invoiked for this object
                        with no argument.
    Author:             christine@netscape.com
    Date:               7 october 1997
*/

    var SECTION = "15.4.4.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array.prototype.toString";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Array.prototype.toString.length",  0,  Array.prototype.toString.length );

    array[item++] = new TestCase( SECTION,  "(new Array()).toString()",     "",     (new Array()).toString() );
    array[item++] = new TestCase( SECTION,  "(new Array(2)).toString()",    ",",    (new Array(2)).toString() );
    array[item++] = new TestCase( SECTION,  "(new Array(0,1)).toString()",  "0,1",  (new Array(0,1)).toString() );
    array[item++] = new TestCase( SECTION,  "(new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toString()",  "NaN,Infinity,-Infinity",   (new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toString() );

    array[item++] = new TestCase( SECTION,  "(new Array( Boolean(1), Boolean(0))).toString()",   "true,false",   (new Array(Boolean(1),Boolean(0))).toString() );
    array[item++] = new TestCase( SECTION,  "(new Array(void 0,null)).toString()",    ",",    (new Array(void 0,null)).toString() );

    array[item++] = new TestCase( SECTION,
                                  "{__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', length: 3}.toString()",
                                  "a,b,c",
                                  {__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', length: 3}.toString() );
    array[item++] = new TestCase( SECTION,
                                  "{__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', join: function() { return 'join' }}.toString()",
                                  "join",
                                  {__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', join: function() { return 'join' }}.toString() );
    array[item++] = new TestCase( SECTION,
                                  "Array.prototype.toString.call({join: function() { return 'join' }})",
                                  "join",
                                  Array.prototype.toString.call({join: function() { return 'join' }}) );
    array[item++] = new TestCase( SECTION,
                                  "Array.prototype.toString.call({sort: function() { return 'sort' }})",
                                  "[object Object]",
                                  Array.prototype.toString.call({sort: function() { return 'sort' }}) );
    array[item++] = new TestCase( SECTION,
                                  "Array.prototype.toString.call(new Date)",
                                  "[object Date]", 
                                  Array.prototype.toString.call(new Date) );
    array[item++] = new TestCase( SECTION,
                                  "Number.prototype.join = function() { return 'number join' }; Array.prototype.toString.call(42)",
                                  "number join",
                                  eval("Number.prototype.join = function() { return 'number join' }; Array.prototype.toString.call(42)") );

    var EXPECT_STRING = "";
    var MYARR = new Array();

    for ( var i = -50; i < 50; i+= 0.25 ) {
        MYARR[MYARR.length] = i;
        EXPECT_STRING += i +",";
    }

    EXPECT_STRING = EXPECT_STRING.substring( 0, EXPECT_STRING.length -1 );

    array[item++] = new TestCase( SECTION, "MYARR.toString()",  EXPECT_STRING,  MYARR.toString() );

    array[item++] = new TestCase( SECTION,
                                  "Array.prototype.join = function() { return 'join' }; [0, 1, 2].toString()",
                                  "join",
                                  eval("Array.prototype.join = function() { return 'join' }; [0, 1, 2].toString()") );

    return ( array );
}
function test() {
    for ( tc=0 ; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+ testcases[tc].actual );
        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}
