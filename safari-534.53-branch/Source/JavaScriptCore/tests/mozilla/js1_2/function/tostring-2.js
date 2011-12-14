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
    File Name:          tostring-1.js
    Section:            Function.toString
    Description:

    Since the behavior of Function.toString() is implementation-dependent,
    toString tests for function are not in the ECMA suite.

    Currently, an attempt to parse the toString output for some functions
    and verify that the result is something reasonable.

    This verifies
    http://scopus.mcom.com/bugsplat/show_bug.cgi?id=99212

    Author:             christine@netscape.com
    Date:               12 november 1997
*/

    var SECTION = "tostring-2";
    var VERSION = "JS1_2";
    startTest();
    var TITLE   = "Function.toString()";
    var BUGNUMBER="123444";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();

    var tab = "    ";


var equals = new TestFunction( "Equals", "a, b", tab+ "return a == b;" );
function Equals (a, b) {
    return a == b;
}

var reallyequals = new TestFunction( "ReallyEquals", "a, b",
    ( version() <= 120 )  ? tab +"return a == b;" : tab +"return a === b;" );
function ReallyEquals( a, b ) {
    return a === b;
}

var doesntequal = new TestFunction( "DoesntEqual", "a, b", tab + "return a != b;" );
function DoesntEqual( a, b ) {
    return a != b;
}

var reallydoesntequal = new TestFunction( "ReallyDoesntEqual", "a, b",
    ( version() <= 120 ) ? tab +"return a != b;"  : tab +"return a !== b;" );
function ReallyDoesntEqual( a, b ) {
    return a !== b;
}

var testor = new TestFunction( "TestOr", "a",  tab+"if (a == null || a == void 0) {\n"+
    tab +tab+"return 0;\n"+tab+"} else {\n"+tab+tab+"return a;\n"+tab+"}" );
function TestOr( a ) {
 if ( a == null || a == void 0 )
    return 0;
 else
    return a;
}

var testand = new TestFunction( "TestAnd", "a", tab+"if (a != null && a != void 0) {\n"+
    tab+tab+"return a;\n" + tab+ "} else {\n"+tab+tab+"return 0;\n"+tab+"}" );
function TestAnd( a ) {
 if ( a != null && a != void 0 )
    return a;
 else
    return 0;
}

var or = new TestFunction( "Or", "a, b", tab + "return a | b;" );
function Or( a, b ) {
    return a | b;
}

var and = new TestFunction( "And", "a, b", tab + "return a & b;" );
function And( a, b ) {
    return a & b;
}

var xor = new TestFunction( "XOr", "a, b", tab + "return a ^ b;" );
function XOr( a, b ) {
    return a ^ b;
}

    testcases[testcases.length] = new TestCase( SECTION,
        "Equals.toString()",
        equals.valueOf(),
        Equals.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "ReallyEquals.toString()",
        reallyequals.valueOf(),
        ReallyEquals.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "DoesntEqual.toString()",
        doesntequal.valueOf(),
        DoesntEqual.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "ReallyDoesntEqual.toString()",
        reallydoesntequal.valueOf(),
        ReallyDoesntEqual.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "TestOr.toString()",
        testor.valueOf(),
        TestOr.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "TestAnd.toString()",
        testand.valueOf(),
        TestAnd.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "Or.toString()",
        or.valueOf(),
        Or.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "And.toString()",
        and.valueOf(),
        And.toString() );

    testcases[testcases.length] = new TestCase( SECTION,
        "XOr.toString()",
        xor.valueOf(),
        XOr.toString() );

    test();

function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+
                            testcases[tc].actual );

        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}
function TestFunction( name, args, body ) {
    this.name = name;
    this.arguments = args.toString();
    this.body = body;

    /* the format of Function.toString() in JavaScript 1.2 is:
    /n
    function name ( arguments ) {
        body
    }
    */
    this.value = "\nfunction " + (name ? name : "anonymous" )+
    "("+args+") {\n"+ (( body ) ? body +"\n" : "") + "}\n";

    this.toString = new Function( "return this.value" );
    this.valueOf = new Function( "return this.value" );
    return this;
}
