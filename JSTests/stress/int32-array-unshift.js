//@ runDefault
var x = [2, 1];
Array.prototype.unshift.call(x, 3);
if (x.toString() != "3,2,1")
    throw "Error: bad result: " + describe(x);

