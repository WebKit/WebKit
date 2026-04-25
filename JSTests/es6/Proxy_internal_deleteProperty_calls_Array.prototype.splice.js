function test() {

// Array.prototype.splice -> DeletePropertyOrThrow -> [[Delete]]
var del = [];
var p = new Proxy([0,0,0,0,,0], { deleteProperty: function(o, v) { del.push(v); return delete o[v]; }});
p.splice(2,2,0);
return del + '' === "3,5";
      
}

if (!test())
    throw new Error("Test failed");

