"use strict";

function createRNG(seed)
{
    return function() {
        // Robert Jenkins' 32 bit integer hash function.
        seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
        seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
        seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
        seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
        seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
        seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
        return (seed & 0xfffffff) / 0x10000000;
    };
}

let random = createRNG(0x7a11bec8);

function mergeSorted(array, cont)
{
    if (array.length <= 1)
        return cont(array);
    
    let midIndex = Math.floor(array.length / 2);
    return mergeSorted(array.slice(0, midIndex), (left) => {
    return mergeSorted(array.slice(midIndex), (right) => {
    
    let result = [];
    
    function finish(source, index)
    {
        if (index >= source.length)
            return;
        result.push(source[index]);
        return finish(source, index + 1);
    }
    
    function merge(leftIndex, rightIndex)
    {
        if (leftIndex >= left.length)
            return finish(right, rightIndex);
        if (rightIndex >= right.length)
            return finish(left, leftIndex);
        
        let leftValue = left[leftIndex];
        let rightValue = right[rightIndex];
        if (leftValue < rightValue) {
            result.push(leftValue);
            return merge(leftIndex + 1, rightIndex);
        }
        result.push(rightValue);
        return merge(leftIndex, rightIndex + 1);
    }
    
    merge(0, 0);
    
    return cont(result);
}); }); }

function checkSorted(array)
{
    function check(index)
    {
        if (index >= array.length - 1)
            return;
        
        if (array[index] > array[index + 1])
            throw new Error("array not sorted at index " + index + ": " + array);
        
        return check(index + 1);
    }
    
    check(0);
}

function checkSpectrum(a, b)
{
    var aMap = new Map();
    var bMap = new Map();
    
    function add(map, value)
    {
        let existing = map.get(value);
        if (existing == null)
            existing = 0;
        map.set(value, existing + 1);
    }
    
    function build(map, array, index)
    {
        if (index >= array.length)
            return;
        
        add(map, array[index]);
        return build(map, array, index + 1);
    }
    
    build(aMap, a, 0);
    build(bMap, b, 0);
    
    function compare(keys)
    {
        let entry = keys.next();
        if (entry.done)
            return;
        if (aMap.get(entry.value) != bMap.get(entry.value))
            throw new Error("arrays don't have the same number of: " + entry.value + " (a = " + a + ", b = " + b + ")");
        return compare(keys);
    }
    
    compare(aMap.keys());
}

function buildArray(length, value)
{
    let array = [];
    
    function build()
    {
        if (array.length >= length)
            return;
        array.push(value(array.length));
        return build();
    }
    
    build();
    
    return array;
}

let arrays = [
    buildArray(10, x => x),
    buildArray(10, x => -x),
    buildArray(10000, x => random())
];

function test(index)
{
    if (index >= arrays.length)
        return;
    
    let array = arrays[index];
    mergeSorted(array, (sorted) => {
    checkSorted(sorted);
    checkSpectrum(array, sorted);
    test(index + 1);
}); }

for (var i = 0; i < 100; ++i)
    test(0);
