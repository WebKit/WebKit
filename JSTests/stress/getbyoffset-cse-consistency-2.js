
let a = {"x": 3};
a.y1 = 3.3;

let b = {};
let res = 42;
b.__defineGetter__("y2", function () {
    res = 43;
    return {};
});
b.x = 1.1;

let c = {};
c.slot_1 = 1.1;

function func(flag, arg0, arg1) {
    let tmp = b;
    if (flag) {
        tmp = a;
    }
    if (arg1 > 100) {
        return;
    }
    for (let i = 0; i < 2; i++) {
        if (flag) {
            var x = tmp.x;
        } else {
            var x = tmp.x;
        }
        arg0.y = 3.3;
    }
    "" + x;
    c.slot_1 = x;
}

for (let i = 0; i < 0x100000; i++) {
    func(i % 2, {}, i);
}

b.x
func(0, {}, 1);
c.slot_1

if (res == 43)
    throw "BAD!";
