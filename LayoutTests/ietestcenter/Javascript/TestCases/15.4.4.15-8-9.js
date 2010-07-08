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
id: "15.4.4.15-8-9",

path: "TestCases/chapter15/15.4/15.4.4/15.4.4.15/15.4.4.15-8-9.js",

description: "Array.prototype.lastIndexOf must return correct index (Sparse Array)",

test: function testcase() {
  var a = new Array(0,1);  
  a[4294967294] = 2;          // 2^32-2 - is max array element index
  a[4294967295] = 3;          // 2^32-1 added as non-array element property
  a[4294967296] = 4;          // 2^32   added as non-array element property
  a[4294967297] = 5;          // 2^32+1 added as non-array element property
  // stop searching near the end in case implementation actually tries to test all missing elements!!
  a[4294967200] = 3;          
  a[4294967201] = 4;         
  a[4294967202] = 5;         


  return (a.lastIndexOf(2) === 4294967294 &&    
      a.lastIndexOf(3) === 4294967200 &&
      a.lastIndexOf(4) === 4294967201 &&
      a.lastIndexOf(5) === 4294967202) ;
   },

precondition: function prereq() {
  return fnExists(Array.prototype.lastIndexOf);
 }
});
