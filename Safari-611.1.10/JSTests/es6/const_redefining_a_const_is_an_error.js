function test() {

const baz = 1;
try {
  Function("const foo = 1; foo = 2;")();
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

