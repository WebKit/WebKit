// This test passes if it does not crash.

var arr0 = [42];
var arr4 = [,,,,,,,,,,,,,,,,,,,,,,,,];

new Array(10000).map((function() {
    arr4[-35] = 1.1;
}), this);

arr0[0] = [];
gc();

Array.prototype.__proto__ = {};
gc();

for(var i = 0; i < 65536; i++)
    arr0['a'+i] = 1.1;
