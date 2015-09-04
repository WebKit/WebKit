function test() {

var [a,b] = [5,6], {c,d} = {c:7,d:8};
return a === 5 && b === 6 && c === 7 && d === 8;
      
}

if (!test())
    throw new Error("Test failed");

