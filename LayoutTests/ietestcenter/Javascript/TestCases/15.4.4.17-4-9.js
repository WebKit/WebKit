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



ES5Harness.registerTest( {
id: "15.4.4.17-4-9",

path: "TestCases/chapter15/15.4/15.4.4/15.4.4.17/15.4.4.17-4-9.js",

description: "Array.prototype.some returns false if 'length' is 0 (subclassed Array, length overridden with [0]",

test: function testcase() {
  foo.prototype = new Array(1, 2, 3);
  function foo() {}
  var f = new foo();
  
  f.length = [0];
  
  // objects inherit the default valueOf method of the Object object;
  // that simply returns the itself. Since the default valueOf() method
  // does not return a primitive value, ES next tries to convert the object
  // to a number by calling its toString() method and converting the
  // resulting string to a number.
  //
  // The toString( ) method on Array converts the array elements to strings,
  // then returns the result of concatenating these strings, with commas in
  // between. An array with no elements converts to the empty string, which
  // converts to the number 0. If an array has a single element that is a
  // number n, the array converts to a string representation of n, which is
  // then converted back to n itself. If an array contains more than one element,
  // or if its one element is not a number, the array converts to NaN.

  function cb(){ return true}
  var i = f.some(cb);
  
  return !i
 },

precondition: function prereq() {
  return fnExists(Array.prototype.some);
 }
});
