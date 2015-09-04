function test() {

try {
  RegExp.prototype.source; return false;
} catch(e) {}
try {
  Date.prototype.valueOf(); return false;
} catch(e) {}
return true;
      
}

if (!test())
    throw new Error("Test failed");

