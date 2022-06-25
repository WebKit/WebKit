//@ runDefault
var x = ['b', 'a'];
Array.prototype.unshift.call(x, 'c');
if (x.toString() != "c,b,a")
    throw "Error: bad result: " + describe(x);

