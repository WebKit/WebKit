function test() {

// Array.prototype.concat -> Get -> [[Get]]
var get = [];
var arr = [1];
arr.constructor = undefined;
var p = new Proxy(arr, { get: function(o, k) { get.push(k); return o[k]; }});
Array.prototype.concat.call(p,p);
return get[0] === "constructor"
  && get[1] === Symbol.isConcatSpreadable
  && get[2] === "length"
  && get[3] === "0"
  && get[4] === get[1] && get[5] === get[2] && get[6] === get[3]
  && get.length === 7;
      
}

if (!test())
    throw new Error("Test failed");

