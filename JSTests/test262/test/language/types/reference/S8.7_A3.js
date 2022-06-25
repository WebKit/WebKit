// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Changing the Reference of an Object While Maintaining Integrity
es5id: 8.7_A3
description: >
    Create a reference to the array, and redefine original array with
    new array
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#
// Set items to an array (object) of strings
var items = new Array( "one", "two", "three" );
// Set itemsRef to a reference to items
//
var itemsRef = items;
// Set items to equal a new object
//
items = new Array( "new", "array" );
// items and itemsRef now point to different objects.
// items points to new Array( "new", "array" )
// itemsRef points to new Array( "one", "two", "three" )
if( items == itemsRef ){
  throw new Test262Error('#1: var items = new Array( "one", "two", "three" ); var itemsRef = items; items = new Array( "new", "array" ); items != itemsRef');
};
//
//////////////////////////////////////////////////////////////////////////////
