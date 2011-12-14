/* ***** BEGIN LICENSE BLOCK *****
 *
 * Copyright (C) 1998 Netscape Communications Corporation.
 * Copyright (C) 2010 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * ***** END LICENSE BLOCK ***** */

gTestfile = 'string-001.js';

/**
 *  java.lang.String objects "inherit" JS string methods
 *
 *
 */
var SECTION = "java.lang.Strings using JavaScript String methods";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 " + SECTION;

startTest();

var jm = getMethods( "java.lang.String" );
var methods = new Array();

for ( var i = 0; i < jm.length; i++ ) {
  cm = jm[i].toString();
  methods[methods.length] = [ getMethodName(cm), getArguments(cm) ];
}

var a = new Array();

// These are methods of String.prototype  that differ from existing
// methods of java.lang.String in argument number or type, and but
// according to scott should still be overriden by java.lang.String
// methods. valueOf

a[a.length] = new TestObject(
  "s"+a.length+" = new java.lang.String(\"hello\"); s"+a.length+".valueOf("+a.length+") +''",
  "s"+a.length,
  "valueOf",
  1,
  'false',
  '"0.0"' );

// These are methods of String.prototype that should be overriden
// by methods of java.lang.String:
// toString  charAt indexOf lastIndexOf substring substring(int, int)
// toLowerCase toUpperCase

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"boo\"); s"+a.length+".toString() +''",
  "s"+a.length,
  "toString",
  0,
  'false',
  '"boo"' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".charAt(0)",
  "s"+a.length,
  "charAt",
  1,
  'false',
  '"J".charCodeAt(0)' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".indexOf(\"L\")",
  "s"+a.length,
  "indexOf",
  1,
  'false',
  '11' );


a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".lastIndexOf(\"t\")",
  "s"+a.length,
  "lastIndexOf",
  1,
  'false',
  '21' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".substring(\"11\") +''",
  "s"+a.length,
  "substring",
  1,
  'false',
  '"LiveConnect"' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".substring(\"15\") +''",
  "s"+a.length,
  "substring",
  1,
  'false',
  '"Connect"' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".substring(4,10) +''",
  "s"+a.length,
  "substring",
  2,
  'false',
  '"Script"' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".toLowerCase() +''",
  "s"+a.length,
  "substring",
  0,
  'false',
  '"javascript liveconnect"' );

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"JavaScript LiveConnect\"); s"+a.length+".toUpperCase() +''",
  "s"+a.length,
  "substring",
  0,
  'false',
  '"JAVASCRIPT LIVECONNECT"' );

// These are methods of String.prototype but are not methods of
// java.lang.String, so they should not be overriden.  The method
// of the instance should be the same as the method of String.prototype
// fromCharCode charCodeAt constructor split

/* No longer valid in JDK 1.4: java.lang.String now has a split method.

a[a.length] = new TestObject(
"s" +a.length+" = new java.lang.String(\"0 1 2 3 4 5 6 7 8 9\"); s"+a.length+".split(\" \") +''",
"s"+a.length,
"split",
0,
'true',
'"0,1,2,3,4,5,6,7,8,9"' );
*/

a[a.length] = new TestObject(
  "s" +a.length+" = new java.lang.String(\"0 1 2 3 4 5 6 7 8 9\"); s"+a.length+".constructor",
  "s"+a.length,
  "constructor",
  0,
  'true',
  'String.prototype.constructor');


// figure out what methods exist
// if there is no java method with the same name as a js method, should
// be able to invoke the js method without casting to a js string.  also
// the method should equal the same method of String.prototype.
// if there is a java method with the same name as a js method, invoking
// the method should call the java method

function TestObject( description, ob, method, argLength, override, expect ) {
  this.description = description;
  this.object = ob;
  this.method = method;
  this.override = override
    this.argLength = argLength;
  this.expect;

  shouldBeWithErrorCheck(
    description,
    expect);

    // FIXME: Why are these branches identical?
  if ( hasMethod( method, argLength )  ) {
    shouldBeWithErrorCheck(
      ob +"." + method +" == String.prototype." + method,
      override);

  } else  {
    // If the java class has no method with that name and number of
    // arguments, the value of the method should be the value of
    // String.prototype.methodName

    shouldBeWithErrorCheck(
      ob +"." + method +" == String.prototype." + method,
      override);
  }
}

function getMethods( javaString ) {
  try {
    return java.lang.Class.forName( javaString ).getMethods();
  } catch (ex) {
    testFailed("Cannot get methods for " + javaString + ": " + ex);
    return [];
  }
}
function isStatic( m ) {
  if ( m.lastIndexOf("static") > 0 ) {
    // static method, return true
    return true;
  }
  return false;
}
function getArguments( m ) {
  if (typeof m.length != "function")
    return "Not a method";
  var argIndex = m.lastIndexOf("(", m.length());
  var argString = m.substr(argIndex+1, m.length() - argIndex -2);
  return argString.split( "," );
}
function getMethodName( m ) {
  if (typeof m.length != "function")
    return "Not a method";
  var argIndex = m.lastIndexOf( "(", m.length());
  var nameIndex = m.lastIndexOf( ".", argIndex);
  return m.substr( nameIndex +1, argIndex - nameIndex -1 );
}
function hasMethod( m, noArgs ) {
  for ( var i = 0; i < methods.length; i++ ) {
    if ( (m == methods[i][0]) && (noArgs == methods[i][1].length)) {
      return true;
    }
  }
  return false;
}
