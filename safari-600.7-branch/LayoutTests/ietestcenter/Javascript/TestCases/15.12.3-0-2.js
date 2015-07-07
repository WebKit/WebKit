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
This test should be run without any built-ins being added/augmented.
The name JSON must be bound to an object.

Section 15 says that every built-in Function object described in this
section — whether as a constructor, an ordinary function, or both — has
a length property whose value is an integer. Unless otherwise specified,
this value is equal to the largest number of named arguments shown in
the section headings for the function description, including optional
parameters.

This default applies to JSON.stringify, and it must exist as a function
taking 3 parameters.
*/


ES5Harness.registerTest( {
id: "15.12.3-0-2",

path: "TestCases/chapter15/15.12/15.12.3/15.12.3-0-2.js",

description: "JSON.stringify must exist as be a function taking 3 parameters",

test: function testcase() {
  var f = JSON.stringify;

  if (typeof(f) === "function" && f.length === 3) {
    return true;
  }
 },

 precondition: function preq () {
  return JSON !== undefined;
  }
});
