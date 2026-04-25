/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
"use strict";

function isRepresentableAsInt32(value)
{
    return (value | 0) === value;
}

function addIndexed(list, cons, ...args)
{
    let result = new cons(list.length, ...args);
    list.push(result);
    return result;
}

const stackAlignmentBytes = 16;

function roundUpToMultipleOf(amount, value)
{
    return Math.ceil(value / amount) * amount;
}

function symbolName(symbol)
{
    let fullString = symbol.toString();
    return fullString.substring("Symbol(".length, fullString.length - ")".length);
}

function lowerSymbolName(symbol)
{
    return symbolName(symbol).toLowerCase();
}

function setToString(set)
{
    let result = "";
    for (let value of set) {
        if (result)
            result += ", ";
        result += value;
    }
    return result;
}

function mergeIntoSet(target, source)
{
    let didAdd = false;
    for (let value of source) {
        if (target.has(value))
            continue;
        target.add(value);
        didAdd = true;
    }
    return didAdd;
}

function nonEmptyRangesOverlap(leftMin, leftMax, rightMin, rightMax)
{
    if (leftMin >= leftMax)
        throw new Error("Bad left range");
    if (rightMin >= rightMax)
        throw new Error("Bad right range");
    
    if (leftMin <= rightMin && leftMax > rightMin)
        return true;
    if (rightMin <= leftMin && rightMax > leftMin)
        return true;
    return false;
}

function rangesOverlap(leftMin, leftMax, rightMin, rightMax)
{
    if (leftMin > leftMax)
        throw new Error("Bad left range");
    if (rightMin > rightMax)
        throw new Error("Bad right range");
    
    if (leftMin == leftMax)
        return false;
    if (rightMin == rightMax)
        return false;
    
    return nonEmptyRangesOverlap(leftMin, leftMax, rightMin, rightMax);
}

function removeAllMatching(array, func)
{
    let srcIndex = 0;
    let dstIndex = 0;
    while (srcIndex < array.length) {
        let value = array[srcIndex++];
        if (!func(value))
            array[dstIndex++] = value;
    }
    array.length = dstIndex;
}

function bubbleSort(array, lessThan)
{
    function swap(i, j)
    {
        var tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
    
    let begin = 0;
    let end = array.length;
    for (;;) {
        let changed = false;
        
        function bubble(i, j)
        {
            if (lessThan(array[i], array[j])) {
                swap(i, j);
                changed = true;
            }
        }
    
        if (end < begin)
            throw new Error("Begin and end are messed up");
        
        let limit = end - begin;
        for (let i = limit; i-- > 1;)
            bubble(begin + i, begin + i - 1);
        if (!changed)
            return;
        
        // After one run, the first element in the list is guaranteed to be the smallest.
        begin++;
        
        // Now go in the other direction. This eliminates most sorting pathologies.
        changed = false;
        
        if (end < begin)
            throw new Error("Begin and end are messed up");
        
        limit = end - begin;
        for (let i = 1; i < limit; ++i)
            bubble(begin + i, begin + i - 1);
        if (!changed)
            return;
        
        // Now the last element is guaranteed to be the largest.
        end--;
    }
}

let currentTime;
if (this.performance && performance.now)
    currentTime = function() { return performance.now() };
else if (this.preciseTime)
    currentTime = function() { return preciseTime() * 1000; };
else
    currentTime = function() { return +new Date(); };

