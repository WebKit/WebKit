//@ skip if $memoryLimited or ($architecture != "arm64" && $architecture != "x86_64")

let giga = 1024 * 1024 * 1024;

let array = new Int8Array(4 * giga);
array[0] = 1;
array[giga] = 2;
array[2 * giga] = 3;
array[3 * giga] = 4;

function expect(base, index, expected, string)
{
    let result = base [index]
    if (result != expected)
        throw "Expected " + expected + " but got " + result + " while testing " + string;
}

let slice0 = array.slice(giga);
expect(slice0, 0, 2, "slice0");
expect(slice0, giga, 3, "slice0");
expect(slice0, 2*giga, 4, "slice0");

let subslice0 = slice0.slice(giga);
expect(subslice0, 0, 3, "subslice0");
expect(subslice0, giga, 4, "subslice0");

let slice1 = array.slice(giga, 2 * giga);
expect(slice1, 0, 2, "slice1");

let slice2 = array.slice(3 * giga);
expect(slice2, 0, 4, "slice2");

let slice3 = array.slice(4 * giga);
let subSlice3 = slice3.slice(0);

