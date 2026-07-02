function test() {

// Object.prototype.hasOwnProperty -> HasOwnProperty -> [[GetOwnProperty]]
var gopd = [];
var p = new Proxy({foo:1, bar:2},
  { getOwnPropertyDescriptor: function(o, v) { gopd.push(v); return Object.getOwnPropertyDescriptor(o, v); }});
p.hasOwnProperty('garply');
return gopd + '' === "garply";
      
}

if (!test())
    throw new Error("Test failed");

