function func1(x) {
    let sum = 0;
    for (let i = 0; i < x; i++) {
        sum += i;
    }
    return sum;
}
noInline(func1);

function test(func) {
    let expected;
    for (let i = 0; i < testLoopCount; i++) {
        let res = func(4);
        if (i == 0)
            expected = res;
        if (expected != res) {
            throw new Error("bad! expected=", expected, " res=", res);
        }
    }
}

test(func1);
