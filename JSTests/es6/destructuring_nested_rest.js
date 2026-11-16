function test() {

var a = [1, 2, 3], first, last;
[first, ...[a[2], last]] = a;
return first === 1 && last === 3 && (a + "") === "1,2,2";
      
}

if (!test())
    throw new Error("Test failed");

