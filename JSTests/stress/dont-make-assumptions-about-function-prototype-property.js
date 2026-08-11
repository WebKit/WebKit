
equals = function (a, e) {
   if (a !== e)
    throw new Error(`Expected = ${e} but got: ${a}`);
}
exists = function (a) {
   if (a === undefined)
    throw new Error(`Expected = not undefined but got undefined`);
}
notexists = function (a) {
   if (a !== undefined)
    throw new Error(`Expected = undefined but got ${a}`);
}

function get_proto(x) {
    return x.prototype;
}

function delete_proto(x) {
    return delete x.prototype;
}

function round0() {
    function func() {
        return 1;
    }
    const obj = {
        method() {
            return 2;
        },
    }
    exists(get_proto(func));
    exists(get_proto(func));
    notexists(get_proto(obj.method));
    notexists(get_proto(obj.method));
}

function testGet() {
    function func() {
        return 1;
    }
    const obj = {
        method() {
            return 2;
        },
    }
    exists(get_proto(func));
    exists(get_proto(func));
    notexists(get_proto(obj.method));
    notexists(get_proto(obj.method));
}

function testDelete() {
    function func() {
        return 1;
    }
    const obj = {
        method() {
            return 2;
        },
    }
    equals(delete_proto(func), false);
    equals(delete_proto(func), false);
    equals(delete_proto(obj), true);
    equals(delete_proto(obj.method), true);
    equals(delete_proto(obj.method), true);
}

round0();

for (let i = 0; i < 10000; i++) {
    testGet();
}
for (let i = 0; i < 10000; i++) {
    testDelete();
}


