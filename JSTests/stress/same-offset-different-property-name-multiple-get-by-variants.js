function opt(wrapper, object, call, get) {
    // CheckStructure
    object.prototype.p1;
    object.prototype.p2;

    if (call) {
        // CheckIsConstant
        wrapper instanceof object;

        if (get) {
            // GetById
            return object.prototype.p1.value;
        }
    }
}

function main() {
    const object1 = function () {};
    const object2 = function () {};
    const object3 = function () {};
    const object4 = {value: 1};

    const wrapper1 = new object1();
    const wrapper2 = new object2();

    // S1
    object1.prototype.p1 = object4;
    object1.prototype.p2 = object4;

    // S1 -> S2
    object2.prototype.p1 = 1;
    object2.prototype.p2 = 1;

    delete object2.prototype.p2;
    delete object2.prototype.p1;

    object2.prototype.p2 = 2;
    object2.prototype.p1 = 2;

    Reflect.defineProperty(object3.prototype, 'p1', {
        get() {
            return object4;
        }
    });

    // Just to force the compiler to emit the GetById node. Otherwise, it'll optimize it into a GetByOffset node.
    opt(wrapper1, object3, /* call */ true, /* get */ true);

    for (let i = 0; i < 1000000; i++) {
        opt(wrapper1, object1, /* call */ true, /* get */ false);
        opt(wrapper1, object2, /* call */ false, /* get */ false);
    }

    // S1 -> S2
    delete object1.prototype.p2;
    delete object1.prototype.p1;

    object1.prototype.p2 = 1;
    object1.prototype.p1 = 0x1234;

    String(opt(wrapper1, object1, /* call */ true, /* get */ true));
}

main();

