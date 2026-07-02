//@ runDefault
var x = [2.5, 1.5];
x.length = 1000000000;
x.length = 2;
Array.prototype.unshift.call(x, 3.5);
if (x.toString() != "3.5,2.5,1.5")
    throw "Error: bad result: " + describe(x);

