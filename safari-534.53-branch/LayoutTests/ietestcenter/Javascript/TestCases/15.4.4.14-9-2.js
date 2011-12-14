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
id: "15.4.4.14-9-2",

path: "TestCases/chapter15/15.4/15.4.4/15.4.4.14/15.4.4.14-9-2.js",

description: "Array.prototype.indexOf must return correct index (Number)",

test: function testcase() {
  var obj = {toString:function(){return 0}};
  var one = 1;
  var _float = -(4/3);
  var a = new Array(false,undefined,null,"0",obj,-1.3333333333333, "str",-0,true,+0, one, 1,0, false, _float, -(4/3));
  if (a.indexOf(-(4/3)) === 14 &&      // a[14]=_float===-(4/3)
      a.indexOf(0) === 7      &&       // a[7] = +0, 0===+0
      a.indexOf(-0) === 7      &&     // a[7] = +0, -0===+0
      a.indexOf(1) === 10 )            // a[10] =one=== 1
  {
    return true;
  }
 },

precondition: function prereq() {
  return fnExists(Array.prototype.indexOf);
 }
});
