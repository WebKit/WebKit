function test() {

// Array.prototype.shift -> DeletePropertyOrThrow -> [[Delete]]
var del = [];
var p = new Proxy([0,,0,,0,0], { deleteProperty: function(o, v) { del.push(v); return delete o[v]; }});
p.shift();
return del + '' === "0,2,5";
      
}

if (!test())
    throw new Error("Test failed");

