// Minimal harness object to run tests from http://samples.msdn.microsoft.com/ietestcenter/#javascript
var ES5Harness = {
    registerTest: (function(test) { this.test = test; }),
    preconditionPassed: false,
    testPassed: false,
};

// The following code is taken from:
// http://samples.msdn.microsoft.com/ietestcenter/Javascript/SimpleTestHarness/sth.js

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

// ----------------------------------------------
// helpers that unittests can use (typically in
// their prereq function).
// ----------------------------------------------
function fnExists(f) {
  if (typeof(f) === "function") {
    return true;
  }
}

var supportsStrict = undefined;
function fnSupportsStrict() {
   "use strict";
   if (supportsStrict!==undefined) return supportsStrict;
   try {eval('with ({}) {}'); supportsStrict=false;} catch (e) {supportsStrict=true;};     
   return supportsStrict;
  }

function fnGlobalObject() {
  return (function () {return this}).call(null);
  }


function compareArray(aExpected, aActual) {
  if (aActual.length != aExpected.length) {
    return false;
  }

  aExpected.sort();
  aActual.sort();

  var s;
  for (var i = 0; i < aExpected.length; i++) {
    if (aActual[i] != aExpected[i]) {
      return false;
    }
  }
  
  return true;
}


function arrayContains(arr, expected) {
  var found;

  for(var i = 0; i < expected.length; i++) {
    found = false;

    for(var j = 0; j < arr.length; j++)
      if(expected[i] === arr[j]) {
        found = true;
        break;
      }

    if(!found)
      return false
  }

  return true;
}