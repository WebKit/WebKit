// Copyright (c) 2001-2010, Purdue University. All rights reserved.
// Copyright (C) 2015 Apple Inc. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the Purdue University nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function compareNumbers(a, b) {
    if (a == b)
        return 0;
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    
    // We say that NaN is smaller than non-NaN.
    if (a == a)
        return 1;
    return -1;
}

function averageAbovePercentile(numbers, percentile) {
    // Don't change the original array.
    numbers = numbers.slice();
    
    // Sort in ascending order.
    numbers.sort(function(a, b) { return a - b; });
    
    // Now the elements we want are at the end. Keep removing them until the array size shrinks too much.
    // Examples assuming percentile = 99:
    //
    // - numbers.length starts at 100: we will remove just the worst entry and then not remove anymore,
    //   since then numbers.length / originalLength = 0.99.
    //
    // - numbers.length starts at 1000: we will remove the ten worst.
    //
    // - numbers.length starts at 10: we will remove just the worst.
    var numbersWeWant = [];
    var originalLength = numbers.length;
    while (numbers.length / originalLength > percentile / 100)
        numbersWeWant.push(numbers.pop());
    
    var sum = 0;
    for (var i = 0; i < numbersWeWant.length; ++i)
        sum += numbersWeWant[i];
    
    var result = sum / numbersWeWant.length;
    
    // Do a sanity check.
    if (numbers.length && result < numbers[numbers.length - 1]) {
        throw "Sanity check fail: the worst case result is " + result +
            " but we didn't take into account " + numbers;
    }
    
    return result;
}

var currentTime;
if (this.performance && performance.now)
    currentTime = function() { return performance.now() };
else if (preciseTime)
    currentTime = function() { return preciseTime() * 1000; };
else
    currentTime = function() { return 0 + new Date(); };
