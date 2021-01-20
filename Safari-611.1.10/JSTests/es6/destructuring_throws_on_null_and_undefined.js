function test() {

try {
  var {a} = null;
  return false;
} catch(e) {}
try {
  var {b} = undefined;
  return false;
} catch(e) {}
return true;
      
}

if (!test())
    throw new Error("Test failed");

