function test() {

// Array.prototype methods -> Get -> [[Get]]
var methods = ['copyWithin', 'every', 'fill', 'filter', 'find', 'findIndex', 'forEach',
  'indexOf', 'join', 'lastIndexOf', 'map', 'reduce', 'reduceRight', 'some'];
var get;
var p = new Proxy({length: 2, 0: '', 1: ''}, { get: function(o, k) { get.push(k); return o[k]; }});
for(var i = 0; i < methods.length; i+=1) {
  get = [];
  Array.prototype[methods[i]].call(p, Function());
  if (get + '' !== (
    methods[i] === 'fill' ? "length" :
    methods[i] === 'every' ? "length,0" :
    methods[i] === 'lastIndexOf' || methods[i] === 'reduceRight' ? "length,1,0" :
    "length,0,1"
  )) {
    return false;
  }
}
return true;
      
}

if (!test())
    throw new Error("Test failed");

