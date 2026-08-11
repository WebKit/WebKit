function test(arg_array) {


    let new_array = (() => {
        let res = new Array(8);
        res.length
        return res;
    })();
    let o1 = {};
    new_array[0] = o1;

    let f;
    if (arg_array[1]) {
        new_array[0] = new_array.length;
        f = new_array;
    } else {
        f = 42;
    }

    var obj = { 'f': f };
}

for (let j = 0; j < 1e5; j++) {
    test([1.1, 2.2]);
}
