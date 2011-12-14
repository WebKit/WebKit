description(
"This test checks the behavior of the various array enumeration functions in certain edge case scenarios"
);

var functions = ["every", "forEach", "some", "filter", "reduce", "map", "reduceRight"];
var forwarders = [
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(prev, elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(elem, index, array) { return currentFunc.call(this, elem, index, array); },
    function(prev, elem, index, array) { return currentFunc.call(this, elem, index, array); }
];

function toObject(array) {
    var o = {};
    for (var i in array)
        o[i] = array[i];
    o.length = array.length;
    return o;
}
function toUnorderedObject(array) {
    var o = {};
    var props = [];
    for (var i in array)
        props.push(i);
    for (var i = props.length - 1; i >= 0; i--)
        o[props[i]] = array[props[i]];
    o.length = array.length;
    return o;
}
function returnFalse() { count++; return false; }
function returnTrue() { count++; return true; }
function returnElem(elem) { count++; return elem; }
function returnIndex(a, index) { if (lastIndex >= index) throw "Unordered traversal"; lastIndex = index; count++; return index; }
function increaseLength(a, b, array) { count++; array.length++; }
function decreaseLength(a, b, array) { count++; array.length--; }
function halveLength(a, b, array) { count++; if (!array.halved) array.length = (array.length / 2) | 0; array.halved = true; }

var testFunctions = ["returnFalse", "returnTrue", "returnElem", "returnIndex", "increaseLength", "decreaseLength", "halveLength"];

var simpleArray = [0,1,2,3,4,5];
var emptyArray = [];
var largeEmptyArray = new Array(300);
var largeSparseArray = [0,1,2,3,4,5];
largeSparseArray[299] = 299;

var arrays = ["simpleArray", "emptyArray", "largeEmptyArray", "largeSparseArray"];
function copyArray(a) {
    var g = [];
    for (var i in a)
        g[i] = a[i];
    return g;
}

// Test object and array behaviour matches
for (var f = 0; f < functions.length; f++) {
    for (var t = 0; t < testFunctions.length; t++) {
        for (var a = 0; a < arrays.length; a++) {
            var functionName = functions[f];
            currentFunc = this[testFunctions[t]];
            if (arrays[a] === "largeEmptyArray" && functionName === "map")
                continue;
            if (currentFunc === returnIndex && functionName === "reduceRight")
                continue;
            shouldBe("count=0;lastIndex=-1;copyArray("+arrays[a]+")."+functionName+"(forwarders[f], "+testFunctions[t]+", 0)",
                     "count=0;lastIndex=-1;Array.prototype."+functionName+".call(toObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0)");
        }
    }
}

// Test unordered object and array behaviour matches
for (var f = 0; f < functions.length; f++) {
    for (var t = 0; t < testFunctions.length; t++) {
        for (var a = 0; a < arrays.length; a++) {
            var functionName = functions[f];
            currentFunc = this[testFunctions[t]];
            if (arrays[a] === "largeEmptyArray" && functionName === "map")
                continue;
            if (currentFunc === returnIndex && functionName === "reduceRight")
                continue;
            shouldBe("count=0;lastIndex=-1;copyArray("+arrays[a]+")."+functionName+"(forwarders[f], "+testFunctions[t]+", 0)",
                     "count=0;lastIndex=-1;Array.prototype."+functionName+".call(toUnorderedObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0)");
        }
    }
}

// Test number of function calls
var callCounts = [
[[1,0,0,1],[6,0,0,7],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[1,0,0,1],[2,0,0,2],[2,0,0,2],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[3,0,0,6],[3,0,0,6]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[4,0,0,7]]
];                                                                             
var objCallCounts = [                                                          
[[1,0,0,1],[6,0,0,7],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1],[1,0,0,1]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[1,0,0,1],[2,0,0,2],[2,0,0,2],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]],
[[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7],[6,0,0,7]]
];
for (var f = 0; f < functions.length; f++) {
    for (var t = 0; t < testFunctions.length; t++) {
        for (var a = 0; a < arrays.length; a++) {
            var functionName = functions[f];
            currentFunc = this[testFunctions[t]];
            if (currentFunc === returnIndex && functionName === "reduceRight")
                continue;
            var expectedCnt = "" + callCounts[f][t][a];
            shouldBe("count=0;lastIndex=-1;copyArray("+arrays[a]+")."+functionName+"(forwarders[f], "+testFunctions[t]+", 0); count", expectedCnt);
            var expectedCnt = "" + objCallCounts[f][t][a];
            shouldBe("count=0;lastIndex=-1;Array.prototype."+functionName+".call(toObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0); count", expectedCnt);
            shouldBe("count=0;lastIndex=-1;Array.prototype."+functionName+".call(toObject("+arrays[a]+"), forwarders[f], "+testFunctions[t]+", 0); count", expectedCnt);
        }
    }
}

successfullyParsed = true;
