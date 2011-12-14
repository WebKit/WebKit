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
PutValue operates only on references (see step 3.b).
*/


ES5Harness.registerTest( {
id: "11.13.1-4-1",

path: "TestCases/chapter11/11.13/11.13.1/11.13.1-4-1.js",

description: "simple assignment creates property on the global object if LeftHandSide is an unresolvable reference",

test: function testcase() {
  function foo() {
    __ES3_1_test_suite_test_11_13_1_unique_id_3__ = 42;
  }
  foo();
  
  // in the browser, the "window" serves as the global object.
  var desc = Object.getOwnPropertyDescriptor(window, '__ES3_1_test_suite_test_11_13_1_unique_id_3__');
  if (desc.value === 42 &&
      desc.writable === true &&
      desc.enumerable === true &&
      desc.configurable === true) {
    delete __ES3_1_test_suite_test_11_13_1_unique_id_3__;
    return true;
  }  
 },

precondition: function () {
  return fnExists(Object.getOwnPropertyDescriptor);
 }

});
