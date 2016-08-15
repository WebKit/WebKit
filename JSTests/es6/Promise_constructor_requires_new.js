function test() {

new Promise(function(){});
try {
  Promise(function(){});
  return false;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

