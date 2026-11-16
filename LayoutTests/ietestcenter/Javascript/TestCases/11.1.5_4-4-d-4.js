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
  Refer 11.1.5; 
  The production
    PropertyNameAndValueList :  PropertyNameAndValueList , PropertyAssignment
  4. If previous is not undefined then throw a SyntaxError exception if any of the following conditions are true
    d. IsAccessorDescriptor(previous) is true and IsAccessorDescriptor(propId.descriptor) is true and either both previous and propId.descriptor have [[Get]] fields or both previous and propId.descriptor have [[Set]] fields
*/

ES5Harness.registerTest( {
id: "11.1.5_4-4-d-4",

path: "TestCases/chapter11/11.1/11.1.5/11.1.5_4-4-d-4.js",

description: "Object literal - SyntaxError for duplicate property name (set,get,set)",

test: function testcase() {
  try
  {
    eval("({set foo(arg){}, get foo(){}, set foo(arg1){}});");
  }
  catch(e)
  {
    if(e instanceof SyntaxError)
      return true;
  }
 },

precondition: function () {
   //accessor properties in object literals must be allowed
  try {eval("({set foo(x) {}, get foo(){}});");}
  catch(e) {return false}
  return true;
}

});
