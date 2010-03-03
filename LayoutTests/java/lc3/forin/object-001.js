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

gTestfile = 'object-001.js';

/**
 *  Verify that for-in loops can be used with java objects.
 *
 *  Java array members of object instances should be enumerated in
 *  for... in loops.
 *
 */
var SECTION = "for...in";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0";
SECTION;
startTest();

// we just need to know the names of all the expected enumerated
// properties.  we will get the values to the original objects.

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");
var a = new Array();

try {
    a[a.length] = new TestObject(
      dt.PUB_STRING,
      "dt.getString()",
      0,
      "java.lang.String");
} catch (ex) {
    testFailed("java.lang.String: " + ex.toString());
}

try {
    a[a.length] = new TestObject(
      dt.getLongObject(),
      "dt.getLongObject()",
      0,
      "java.lang.Long");
} catch (ex) {
    testFailed("java.lang.Long: " + ex.toString());
}

try {
    a[a.length] = new TestObject(
      new java.awt.Rectangle(5,6,7,8),
      "new java.awt.Rectangle(5,6,7,8)",
      0,
      "java.awt.Rectangle");
} catch (ex) {
    testFailed("java.awt.Rectangle: " + ex.toString());
}

for ( var i = 0; i < a.length; i++ ) {
  // check the value of each indexed array item

  for ( var arrayItem = 0; arrayItem < a[i].items; arrayItem++ )
    shouldBeWithErrorCheck("a[" + i + "].enumedObject['" + arrayItem + "']", "a[i].javaObject[arrayItem]");

  // verify that all non-static  properties are enumerated


  var fieldArray = [];
  try {
    fieldArray = getFields( a[i].javaClass );
  } catch (ex) {
    testFailed("getFields( a[" + i + "].javaClass ): " + ex);
  }

  for ( var fieldIndex = 0; fieldIndex < fieldArray.length; fieldIndex++ ) {
    try {
        var f = fieldArray[fieldIndex] +"";
    } catch (ex) {
        testFailed(fieldArray[fieldIndex] + " " + ex.toString());
    }

    if ( ! isStatic( f ) ) {
      var currentField = getFieldName( f );

      shouldBeWithErrorCheck( 'a[' + i + '].enumedObject["' + currentField + '"]+""', 'a[' + i + '].javaObject["' + currentField + '"] +""');
    }
  }

  // verify that all non-static methods are enumerated

  var methodArray = [];
  try {
    methodArray = getMethods(a[i].javaClass);
  } catch (ex) {
    testFailed("getMethods( a[" + i + "].javaClass ): " + ex);
  }

  for ( var methodIndex = 0; methodIndex < methodArray.length; methodIndex++ ) {
    try {
        var m = methodArray[methodIndex] +"";
    } catch (ex) {
        testFailed(methodArray[methodIndex] + " " + ex.toString());
    }

    if ( ! isStatic(m)  && inClass( a[i].javaClass, m)) {
      var currentMethod = getMethodName(m);

      shouldBeWithErrorCheck('a[' + i + '].enumedObject["' + currentMethod + '"] +""', 'a[' + i + '].javaObject["' + currentMethod + '"] +""');
    }
  }
}


function TestObject( ob, descr, len, jc) {
  this.javaObject= ob;
  this.description = descr;
  this.items    = len;
  this.javaClass = jc;
  this.enumedObject = new enumObject(ob);
}

function enumObject( o ) {
  this.pCount = 0;
  for ( var p in o ) {
    this.pCount++;
    this[p] = o[p];
  }
}

// only return if the method is a method of the class, not an inherited
// method

function inClass( javaClass, m ) {
  var argIndex = m.lastIndexOf( "(", m.length );
  var methodIndex = m.lastIndexOf( ".", argIndex );
  var classIndex = m.lastIndexOf( " ", methodIndex );
  var className =   m.substr(classIndex +1, methodIndex - classIndex -1 );

  if ( className == javaClass ) {
    return true;
  }
  return false;
}
function isStatic( m ) {
  if ( m.lastIndexOf ( "static" ) > 0 ) {
    return true;
  }
  return false;
}
function getMethods( javaString ) {
  return java.lang.Class.forName( javaString ).getMethods();
}
function getArguments( m ) {
  var argIndex = m.lastIndexOf("(", m.length());
  var argString = m.substr(argIndex+1, m.length() - argIndex -2);
  return argString.split( "," );
}
function getMethodName( m ) {
  var argIndex = m.lastIndexOf( "(", m.length);
  var nameIndex = m.lastIndexOf( ".", argIndex);
  return m.substr( nameIndex +1, argIndex - nameIndex -1 );
}
function getFields( javaClassName ) {
  return java.lang.Class.forName( javaClassName ).getFields();
}
function getFieldName( f ) {
  return f.substr( f.lastIndexOf(".", f.length)+1, f.length );
}
function getFieldNames( javaClassName ) {
  var javaFieldArray = getFields( javaClassName );

  for ( var i = 0, jsFieldArray = new Array(); i < javaFieldArray.length; i++ ) {
    var f = javaFieldArray[i] +"";
    jsFieldArray[i] = f.substr( f.lastIndexOf(".", f.length)+1, f.length );
  }
  return jsFieldArray;
}
function hasMethod( m, noArgs ) {
  for ( var i = 0; i < methods.length; i++ ) {
    if ( (m == methods[i][0]) && (noArgs == methods[i][1].length)) {
      return true;
    }
  }
  return false;
}
