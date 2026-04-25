function test() {

try {
  eval("({ __proto__ : [], __proto__: {} })");
}
catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

