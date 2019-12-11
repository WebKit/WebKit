function test1(item) {
    var o = {
        10000: item,
        get 10005() { },
    };
    let arr = new Array(10008);
    for (let key of arr.keys()) {
        let o2 = {};
        o[key] = o2;
    }
}
test1({});
test1(10);
test1(10.5);

function test2(item) {
    var o = {
        0: item,
        get 1000() { },
    };
    let arr = new Array(1000);
    for (let key of arr.keys()) {
        let o2 = {};
        o[key] = o2;
    }
}
test2({});
test2(10);
test2(10.5);
