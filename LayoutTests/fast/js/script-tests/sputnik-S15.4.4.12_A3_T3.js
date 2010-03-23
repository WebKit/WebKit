/*
    Copyright 2009 the Sputnik authors.  All rights reserved.
    Copyright (C) 2010 Apple Inc. All rights reserved.
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    
        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials provided
          with the distribution.
        * Neither the name of Google Inc. nor the names of its
          contributors may be used to endorse or promote products derived
          from this software without specific prior written permission.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

description('Sputnik test S15.4.4.12_A3_T3.');

$ERROR = testFailed;

/**
 * @name: S15.4.4.12_A3_T3;
 * @section: 15.4.4.12;
 * @assertion: Check ToUint32(length) for non Array objects;
 * @description: length is arbitrarily; 
*/

var obj = {};
obj.splice = Array.prototype.splice;
obj[4294967294] = "x";
obj.length = -1;
var arr = obj.splice(4294967294,1);

//CHECK#1
if (arr.length !== 1) {
  $ERROR('#1: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = -1; var arr = obj.splice(4294967294,1); arr.length === 1. Actual: ' + (arr.length));
}

//CHECK#2
if (arr[0] !== "x") {
   $ERROR('#2: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = 1; var arr = obj.splice(4294967294,1); arr[0] === "x". Actual: ' + (arr[0]));
} 

//CHECK#3
if (obj.length !== 4294967294) {
   $ERROR('#3: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = 1; var arr = obj.splice(4294967294,1); obj.length === 4294967294. Actual: ' + (obj.length));
}

//CHECK#4
if (obj[4294967294] !== undefined) {
   $ERROR('#4: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = 1; var arr = obj.splice(4294967294,1); obj[4294967294] === undefined. Actual: ' + (obj[4294967294]));
} 

var successfullyParsed = true;
