function test() {

for(var [i, j, k] in { qux: 1 }) {
  return i === "q" && j === "u" && k === "x";
}
      
}

if (!test())
    throw new Error("Test failed");

