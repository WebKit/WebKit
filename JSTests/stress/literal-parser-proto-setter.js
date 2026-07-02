let fired = false;
Object.prototype.__defineSetter__("__proto__", function(v) {
    if (fired) return;
    fired = true;
    Object.prototype.__defineSetter__(0, function(){});
});

let ks = '"0":null,"1":2,"5":3';

eval("({"+ks+",a:1})");
eval("({"+ks+",a:1})");

let o = eval("({"+ks+",a:{__proto__:0}})");

if (o[1] !== 2) {
    throw new Error("incorrect eval result");
}
