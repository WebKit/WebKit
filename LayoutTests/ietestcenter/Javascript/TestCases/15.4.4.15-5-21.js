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


ES5Harness.registerTest({

    id: "15.4.4.15-5-21",

    path: "TestCases/chapter15/15.4/15.4.4/15.4.4.15/15.4.4.15-5-21.js",

    description: "Array.prototype.lastIndexOf when value of 'fromIndex' which is an Object, and has an own toString method",

    test: function testcase() {

        // objects inherit the default valueOf() method from Object
        // that simply returns itself. Since the default valueOf() method
        // does not return a primitive value, ES next tries to convert the object
        // to a number by calling its toString() method and converting the
        // resulting string to a number.
        var fromIndex = {
            toString: function () {
                return '2';
            }
        };
        var targetObj = new RegExp();

        return [0, true, targetObj, 3, false].lastIndexOf(targetObj, fromIndex) === 2 &&
        [0, true, 3, targetObj, false].lastIndexOf(targetObj, fromIndex) === -1;
    },

    precondition: function prereq() {
        return fnExists(Array.prototype.lastIndexOf);
    }

});
