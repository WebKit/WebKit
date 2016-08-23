//@ runNoFTL
var result = 0;
function func() {
    function C() { 
        this.m = function () {
        	result ^= result * 3 + 5 + (result << 3);
        };
    };
    var a=[];
    for (var i =0; i < 10000; i++) {
        a[i] = (new C);
    }
    a[9000].m = 0.876555555; 
    for (var i = 0; i < 10000; i++)
        a[i].m();
}
try {
	func();
} catch(e) {

}
if (result != 1561806289)
	throw "Expected 1561806289 but got " + result
