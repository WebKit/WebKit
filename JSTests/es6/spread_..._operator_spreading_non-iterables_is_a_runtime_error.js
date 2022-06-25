function test() {

try {
  Math.max(...2);
} catch(e) {
  return Math.max(...[1, 2, 3]) === 3;
}
      
}

if (!test())
    throw new Error("Test failed");

