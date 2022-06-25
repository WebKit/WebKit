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

    id: "15.4.4.15-8-b-i-24",

    path: "TestCases/chapter15/15.4/15.4.4/15.4.4.15/15.4.4.15-8-b-i-24.js",

    description: "Array.prototype.lastIndexOf when to be visited is the document host object",

    test: function testcase() {

        var targetObj = {};
        try {
            document[0] = targetObj;
            document[100] = "100";
            document[200] = "200";
            document.length = 200;
            return 0 === Array.prototype.lastIndexOf.call(document, targetObj) &&
                100 === Array.prototype.lastIndexOf.call(document, "100") &&
                -1 === Array.prototype.lastIndexOf.call(document, "200");
        } finally {
            delete document[0];
            delete document[100];
            delete document[200];
            delete document.length;
        }
    },

    precondition: function prereq() {
        return fnExists(Array.prototype.lastIndexOf);
    }

});
