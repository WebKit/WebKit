function test1(write, escape, value) {
    let arr = new Array(4);

    arr[1] = 1;

    if (write) {
        arr[0] = value + 3;
    }

    if (escape) {
        return arr;
    }
}
noInline(test1);

function test2(write, escape, value) {
    let arr = new Array(4);

    arr[1] = 1.1;

    if (write) {
        arr[0] = value + 3;
    }

    if (escape) {
        return arr;
    }
}
noInline(test2);

function test3(write, escape, value) {
    let arr = new Array(4);

    arr[1] = {};

    if (write) {
        arr[0] = value + 3;
    }

    if (escape) {
        return arr;
    }
}
noInline(test3);

function test4(write, escape, value) {
    let arr = new Array(4);

    arr[1] = {};

    if (write) {
        arr[0] = {
            value: value + 3,
            valueOf() { return this.value; },
        };
    }

    if (escape) {
        return arr;
    }
}
noInline(test4);

function test5(write, escape, value) {
    let arr = new Array(4);

    arr[1] = {};

    if (write) {
        arr[0] = {
            value: value + 3,
            valueOf() { return this.value; },
            // Add a reference cycle.
            arr,
        };
    }

    if (escape) {
        return arr;
    }
}
noInline(test5);

// JIT warmup loop
for(let i = 0; i < testLoopCount; i++) {
    test1(true, false, 3);
    test1(false, true, 3);
    test2(true, false, 3);
    test2(false, true, 3);
    test3(true, false, 3);
    test3(false, true, 3);
    test4(true, false, 3);
    test4(false, true, 3);
    test5(true, false, 3);
    test5(false, true, 3);
}

function check(functor, value) {
    let write = functor(true, true, value);
    if (write[0] != value + 3)
        throw new Error(write);

    let noWrite = functor(false, true, value);
    if (0 in noWrite)
        throw new Error(write);
}

check(test1, 3);
check(test2, 3);
check(test3, 3);
check(test4, 3);
check(test5, 3);
