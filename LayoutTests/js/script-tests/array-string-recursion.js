description("Verify that we do not recurse infinitely through one of the Array->string conversion.");

// Array that only contains itself.
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toString();`, "");
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toLocaleString();`, "");
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.join(",");`, "");

// Array containing itself and a bunch of other objects.
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(1);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push("WebKit!");
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toString();`, "1,,WebKit!,");
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(1);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push("WebKit!");
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.toLocaleString();`, "1,,WebKit!,");
shouldBeEqualToString(`var arrayDirectlyContainingItself = [];
    arrayDirectlyContainingItself.push(1);
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.push("WebKit!");
    arrayDirectlyContainingItself.push(arrayDirectlyContainingItself);
    arrayDirectlyContainingItself.join("-");`, "1--WebKit!-");

// Array indirectly containing itself.
shouldBeEqualToString(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    arrayIndirectlyContainingItself.toString();`, "1,1,2,5,6,,WebKit!");
shouldBeEqualToString(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    arrayIndirectlyContainingItself.toLocaleString();`, "1,1,2,5,6,,WebKit!");
shouldBeEqualToString(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    arrayIndirectlyContainingItself.join("=");`, "1=1,2,5,6,=WebKit!");

// Array containing another array with recursion.
shouldBeEqualToString(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    ["z", arrayIndirectlyContainingItself, 9].toString();`, "z,1,1,2,5,6,,WebKit!,9");
shouldBeEqualToString(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    ["z", arrayIndirectlyContainingItself, 9].toLocaleString();`, "z,1,1,2,5,6,,WebKit!,9");
shouldBeEqualToString(`var arrayIndirectlyContainingItself = [];
    arrayIndirectlyContainingItself.push(1);
    arrayIndirectlyContainingItself.push([1, 2, [5, 6, [arrayIndirectlyContainingItself]]]);
    arrayIndirectlyContainingItself.push("WebKit!");
    ["z", arrayIndirectlyContainingItself, 9].join("&");`, "z&1,1,2,5,6,,WebKit!&9");

// Indirectly recurse to an other of the functions. The object do not contains itself, but contains object that recursively call
// an array to string conversion.
shouldBeEqualToString(`var arrayIndirectlyConvertingItself = ["a"];
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toLocaleString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.join("~") } });
    arrayIndirectlyConvertingItself.push("WebKit!");
    ["z", arrayIndirectlyConvertingItself, 9].toString();`, "z,a,,,,WebKit!,9");
shouldBeEqualToString(`var arrayIndirectlyConvertingItself = ["a"];
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toLocaleString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.join("~") } });
    arrayIndirectlyConvertingItself.push("WebKit!");
    ["z", arrayIndirectlyConvertingItself, 9].toLocaleString();`, "z,a,,,,WebKit!,9");
shouldBeEqualToString(`var arrayIndirectlyConvertingItself = ["a"];
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.toLocaleString() } });
    arrayIndirectlyConvertingItself.push({ array: arrayIndirectlyConvertingItself, toString: function() { return this.array.join("~") } });
    arrayIndirectlyConvertingItself.push("WebKit!");
    ["z", arrayIndirectlyConvertingItself, 9].join("*");`, "z*a,,,,WebKit!*9");
