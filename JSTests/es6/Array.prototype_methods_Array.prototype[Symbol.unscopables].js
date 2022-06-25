function test() {

var unscopables = Array.prototype[Symbol.unscopables];
if (!unscopables) {
  return false;
}
var ns = "find,findIndex,fill,copyWithin,entries,keys,values".split(",");
for (var i = 0; i < ns.length; i++) {
  if (Array.prototype[ns[i]] && !unscopables[ns[i]]) return false;
}
return true;
      
}

if (!test())
    throw new Error("Test failed");

