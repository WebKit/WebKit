// Copyright (c) 2009 Google, Inc.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided
// that the following conditions are met:
//    * Redistributions of source code must retain the above copyright notice, this list of conditions and
//      the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
//      the following disclaimer in the documentation and/or other materials provided with the distribution.
//    * Neither the name of Microsoft nor the names of its contributors may be used to
//      endorse or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


ES5Harness.registerTest( {
id: "15.2.3.14-3-4",

path: "TestCases/chapter15/15.2/15.2.3/15.2.3.14/15.2.3.14-3-4.js",

description: "Object.keys of an arguments object returns the indices of the given arguments",

test: function testcase() {
  function testArgs2(x, y, z) {
    // Properties of the arguments object are enumerable.
    var a = Object.keys(arguments);
    if (a.length === 2 && a[0] === "0" && a[1] === "1")
      return true;
  }
  function testArgs3(x, y, z) {
    // Properties of the arguments object are enumerable.
    var a = Object.keys(arguments);
    if (a.length === 3 && a[0] === "0" && a[1] === "1" && a[2] === "2")
      return true;
  }
  function testArgs4(x, y, z) {
    // Properties of the arguments object are enumerable.
    var a = Object.keys(arguments);
    if (a.length === 4 && a[0] === "0" && a[1] === "1" && a[2] === "2" && a[3] === "3")
      return true;
  }
  return testArgs2(1, 2) && testArgs3(1, 2, 3) && testArgs4(1, 2, 3, 4);
 },

precondition: function prereq() {
  return fnExists(Object.keys);
 }
});