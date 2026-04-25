//@ runDefault
var x = [2.5, 1.5];
Array.prototype.unshift.call(x, 3.5);
if (x.toString() != "3.5,2.5,1.5")
    throw "Error: bad result: " + describe(x);

