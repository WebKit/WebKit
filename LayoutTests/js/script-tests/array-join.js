"use strict";

description("Verify Array.prototype.join() properties");

debug("Function properties");
shouldBeEqualToString("typeof Array.prototype.join", "function");
shouldBeEqualToString("Array.prototype.join.name", "join");
shouldBe("Array.prototype.join.length", "1");
shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype, 'join').configurable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Array.prototype, 'join').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype, 'join').writable");

debug("Int32 Array");
shouldBeEqualToString("[1, 2, 3].join()", "1,2,3");
shouldBeEqualToString("[1, 2, 3].join('')", "123");
shouldBeEqualToString("[1, 2, 3].join('柰')", "1柰2柰3");

debug("Double Array");
shouldBeEqualToString("[Math.PI, Math.E, 6.626].join()", "3.141592653589793,2.718281828459045,6.626");
shouldBeEqualToString("[Math.PI, Math.E, 6.626].join('')", "3.1415926535897932.7182818284590456.626");
shouldBeEqualToString("[Math.PI, Math.E, 6.626].join('柰')", "3.141592653589793柰2.718281828459045柰6.626");

debug("Contiguous Array");
shouldBeEqualToString("[1, 'WebKit', { toString: () => { return 'IsIncredible'} }].join()", "1,WebKit,IsIncredible");
shouldBeEqualToString("[1, 'WebKit', { toString: () => { return 'IsIncredible'} }].join('')", "1WebKitIsIncredible");
shouldBeEqualToString("[1, 'WebKit', { toString: () => { return 'IsIncredible'} }].join('柰')", "1柰WebKit柰IsIncredible");

debug("Sparse Array");
var smallSparseArray = new Array;
smallSparseArray[-1] = "Oops";
smallSparseArray[0] = "WebKit";
smallSparseArray[42] = 15;
shouldBeEqualToString("smallSparseArray.join()", "WebKit,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,15");
shouldBeEqualToString("smallSparseArray.join('')", "WebKit15");
shouldBeEqualToString("smallSparseArray.join('柰')", "WebKit柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰柰15");

var largeSparseArray1 = new Array;
largeSparseArray1[100001] = 42;
largeSparseArray1[0] = "WebKit";
largeSparseArray1[Number.MAX_SAFE_INTEGER] = { valueOf: () => { return 'IsCool'} };
shouldBeEqualToString("largeSparseArray1.join('')", "WebKit42");

var largeSparseArray2 = new Array;
largeSparseArray2[100001] = 42;
largeSparseArray2[42] = "WebKit";
largeSparseArray2[1024] = "";
shouldBeEqualToString("largeSparseArray2.join('')", "WebKit42");

debug("Out of memory");
// 4194303 * 4096 > Max String Length.
let oversizedArray = new Array(4096);
let sharedString = "A".repeat(1048576);
oversizedArray.fill(sharedString);
shouldThrow("oversizedArray.join('')", "'Error: Out of memory'");

debug("ToLength is called first on \"this\", followed by ToString on the separator. Followed by ToString on each element.");
var callSequence = [];
var lengthObject = {
    toString: () => { callSequence.push("length.toString"); return "FAIL!"; },
    valueOf: () => { callSequence.push("length.valueOf"); return 2; }
};
var index0Object = {
    toString: () => { callSequence.push("index0.toString"); return "WebKit0"; },
    valueOf: () => { callSequence.push("index0.valueOf"); return "FAIL!"; }
};
var index1Object = {
    toString: () => { callSequence.push("index0.toString"); return "WebKit1"; },
    valueOf: () => { callSequence.push("index0.valueOf"); return "FAIL!"; }
};
var calleeObject = {
    toString: () => { callSequence.push("callee.toString"); return "FAIL!"; },
    valueOf: () => { callSequence.push("calle.valueOf"); return "FAIL!"; },
    get length () { callSequence.push("calle.length"); return lengthObject; },
    get 0 () { callSequence.push("calle.get 0"); return index0Object; },
    get 1 () { callSequence.push("calle.get 1"); return index1Object; }
};
var separatorObject = {
    toString: () => { callSequence.push("separator.toString"); return "柰"; },
    valueOf: () => { callSequence.push("separator.valueOf"); return "FAIL!"; }
};

shouldBeEqualToString("Array.prototype.join.call(calleeObject, separatorObject)", "WebKit0柰WebKit1");
shouldBeEqualToString("callSequence.join(', ')", "calle.length, length.valueOf, separator.toString, calle.get 0, index0.toString, calle.get 1, index0.toString");


debug("We use ToLength on the object's length, not ToInt32 or ToUInt32.");
var lengthyObject = {
    get 0 () { throw "Fail! Accessed 0."; },
    get 1 () { throw "Fail! Accessed 1."; }
}
lengthyObject.length = -1;
shouldBeEqualToString("Array.prototype.join.call(lengthyObject)", "");
lengthyObject.length = -4294967294;
shouldBeEqualToString("Array.prototype.join.call(lengthyObject)", "");
lengthyObject.length = -4294967295;
shouldBeEqualToString("Array.prototype.join.call(lengthyObject)", "");
