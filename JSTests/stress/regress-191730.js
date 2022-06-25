function assertEq(actual, expected) {
    if (actual != expected)
        throw ("Expected: " + expected + ", actual: " + actual);
}

var otherGlobal = $vm.createGlobalObject();

Array.prototype.__defineSetter__(7, () => {
    arr[0] = { };
});

let arr = new otherGlobal.Array(1.1, 2.2, 3.3);

function foo(arr, regexp, str){
    var result = regexp[Symbol.match](str);
    arr[1] = 3.54484805889626e-310;
    return arr[0];
}

let regexp = /a/g;
for (let i = 0; i < 10000; i++)
    foo(arr, regexp, "aaaa");

let r = foo(arr, regexp, "aaaaaaaa");
assertEq(arr[1], "3.54484805889626e-310");
