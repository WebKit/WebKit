var count = 0;
var arr = new Array(10);
function func(val) {
    if (count++ > 300)
        return;
    try { Object.prototype.__defineGetter__(Symbol.isConcatSpreadable, val);
    } catch(e) {}
    arr[Symbol.iterator] = arr;
    try { arr.concat(arr, new Array(10)); } catch(e) {}
}

for (var i=0; i < 2; i ++) {
    func(new Proxy(func, []));
}
