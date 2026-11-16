function test() {

return (() => {
  try { Function("0 || () => 2")(); } catch(e) { return true; }
})();
      
}

if (!test())
    throw new Error("Test failed");

