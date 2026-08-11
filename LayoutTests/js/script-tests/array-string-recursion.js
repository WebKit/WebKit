description("Verify that an Array->string conversion of an array that reaches itself terminates by throwing a RangeError, rather than recursing infinitely or silently substituting the empty string.");

// Array that only contains itself.
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toString();`);
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toLocaleString();`);
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.join(",");`);

// Array containing itself and a bunch of other objects.
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(1);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push("WebKit!");
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toString();`);
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(1);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push("WebKit!");
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toLocaleString();`);
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(1);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push("WebKit!");
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.join("-");`);

// Array indirectly containing itself.
shouldThrow(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    arrayIndirectlyContainingItself.toString();`);
shouldThrow(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    arrayIndirectlyContainingItself.toLocaleString();`);
shouldThrow(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    arrayIndirectlyContainingItself.join("=");`);

// Array containing another array with recursion.
shouldThrow(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    ["z", arrayIndirectlyContainingItself, 9].toString();`);
shouldThrow(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    ["z", arrayIndirectlyContainingItself, 9].toLocaleString();`);
shouldThrow(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    ["z", arrayIndirectlyContainingItself, 9].join("&");`);

// Indirectly recurse to an other of the functions. The object do not contains itself, but contains object that recursively call
// an array to string conversion.
shouldThrow(`var arrayIndirectlyConvertingItself = ["a"];
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toLocaleString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.join("~") } });
    arrayIndirectlyConvertingItself.push("WebKit!");
    ["z", arrayIndirectlyConvertingItself, 9].toString();`);
shouldThrow(`var arrayIndirectlyConvertingItself = ["a"];
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toLocaleString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.join("~") } });
    arrayIndirectlyConvertingItself.push("WebKit!");
    ["z", arrayIndirectlyConvertingItself, 9].toLocaleString();`);
shouldThrow(`var arrayIndirectlyConvertingItself = ["a"];
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toLocaleString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.join("~") } });
    arrayIndirectlyConvertingItself.push("WebKit!");
    ["z", arrayIndirectlyConvertingItself, 9].join("*");`);

// A cycle that fans out must still terminate: the depth-first descent hits the stack limit and the
// RangeError propagates out instead of the conversion fanning out exponentially.
shouldThrow(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toString();`);

// An acyclic value graph converts normally, even when the same array occurs more than once. A
// repeated occurrence is not recursion, so it must be converted every time rather than skipped.
shouldBeEqualToString(`var shared = [1, 2];
    [shared, shared].toString();`, "1,2,1,2");
shouldBeEqualToString(`var shared = [1, 2];
    [shared, shared].toLocaleString();`, "1,2,1,2");
shouldBeEqualToString(`var shared = [1, 2];
    [shared, shared].join("|");`, "1,2|1,2");

// Recovering from the stack overflow must leave no state behind that suppresses later conversions.
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    try { arrayDirectlyContainingItself.toString(); } catch (e) { }
    [1, 2].toString();`, "1,2");
