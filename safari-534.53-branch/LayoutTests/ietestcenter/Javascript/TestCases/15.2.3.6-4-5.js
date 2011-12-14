/// Copyright (c) 2009 Microsoft Corporation 
/// 
/// Redistribution and use in source and binary forms, with or without modification, are permitted provided
/// that the following conditions are met: 
///    * Redistributions of source code must retain the above copyright notice, this list of conditions and
///      the following disclaimer. 
///    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and 
///      the following disclaimer in the documentation and/or other materials provided with the distribution.  
///    * Neither the name of Microsoft nor the names of its contributors may be used to
///      endorse or promote products derived from this software without specific prior written permission.
/// 
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
/// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
/// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
/// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
/// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
/// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
/// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
/// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

/*
Step 4 of defineProperty calls the [[DefineOwnProperty]] internal method
of O to define the property. Step 6 of [[DefineOwnProperty]] returns if
every field of desc also occurs in current and every field in desc has
the same value as current.
*/

ES5Harness.registerTest( {
id: "15.2.3.6-4-5",

path: "TestCases/chapter15/15.2/15.2.3/15.2.3.6/15.2.3.6-4-5.js",

description: "Object.defineProperty is no-op if current and desc are the same data desc",

test: function testcase() {
  function sameDataDescriptorValues(d1, d2) {
    return (d1.value === d2.value &&
            d1.enumerable === d2.enumerable &&
            d1.writable === d2.writable &&
            d1.configurable === d2.configurable);
  }

  var o = {};

  // create a data valued property with the following attributes:
  // value: 101, enumerable: true, writable: true, configurable: true
  o["foo"] = 101;

  // query for, and save, the desc. A subsequent call to defineProperty
  // with the same desc should not disturb the property definition.
  var d1 = Object.getOwnPropertyDescriptor(o, "foo");  

  // now, redefine the property with the same descriptor
  // the property defintion should not get disturbed.
  var desc = { value: 101, enumerable: true, writable: true, configurable: true };
  Object.defineProperty(o, "foo", desc);

  var d2 = Object.getOwnPropertyDescriptor(o, "foo"); 

  if (sameDataDescriptorValues(d1, d2) === true) {
    return true;
  }
 },

precondition: function prereq() {
  return fnExists(Object.defineProperty) && fnExists(Object.getOwnPropertyDescriptor);
 }
});
