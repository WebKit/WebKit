function test() {

// Array.prototype.reverse -> DeletePropertyOrThrow -> [[Delete]]
var del = [];
var p = new Proxy([0,,2,,4,,], { deleteProperty: function(o, v) { del.push(v); return delete o[v]; }});
p.reverse();
return del + '' === "0,4,2";
      
}

if (!test())
    throw new Error("Test failed");

