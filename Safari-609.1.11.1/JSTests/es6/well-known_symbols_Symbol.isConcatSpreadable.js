function test() {

var a = [], b = [];
b[Symbol.isConcatSpreadable] = false;
a = a.concat(b);
return a[0] === b;
      
}

if (!test())
    throw new Error("Test failed");

