function test() {

var symbol = Symbol();

try {
  symbol + "";
  return false;
}
catch(e) {}

try {
  symbol + 0;
  return false;
} catch(e) {}

return true;
      
}

if (!test())
    throw new Error("Test failed");

