function test() {

return (() => {
  try { Function("x\n => 2")(); } catch(e) { return true; }
})();
      
}

if (!test())
    throw new Error("Test failed");

