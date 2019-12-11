var arr0 = [1,2,3,4];
var arr1 = new Array(1000);

Array.prototype.__defineGetter__(1, function() {
    [].concat(arr1); //generate to invalid JIT code here?
});

Array.prototype.__defineGetter__(Symbol.isConcatSpreadable, (function() {
    for(var i=0;i<10000;i++) {
        if(i==0)
            arr1[i];
        this.x = 1.1;
        arr1.legnth = 1;
    }
}));

var exception;
try {
    arr1[1].toString();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";
