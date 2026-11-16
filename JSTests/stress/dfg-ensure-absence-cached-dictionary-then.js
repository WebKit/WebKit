function createObject1() {
    const tmp = {
        toJSON: 1,
        a: 1,
    };

    Object.create(tmp);

    return tmp;
}

function createObject2() {
    const tmp = {
        b: 1,
        toJSON: {}
    };

    Object.create(tmp);

    return tmp;
}

function opt(container1, object2, array, thenable, flags) {
    const promise = new Promise(() => {});

    container1.x;
    thenable.x;

    const object1 = Object.getPrototypeOf(container1);

    let tmp = object1;
    object1.a;

    if (flags & 1) {
        tmp = object2;
        tmp.b;

        0[0];
    }

    +tmp.toJSON;
    +tmp.toJSON;

    array[0];
    Promise.resolve(+tmp.toJSON === 1 ? thenable : promise);

    array[0] = 2.3023e-320;
}

function main() {
    noDFG(main);

    const object1 = createObject1();
    const object2 = createObject2();

    createObject1().z = 1;

    const container1 = Object.create(object1);

    const thenable = {
        x: 1
    };

    for (let i = 0; i < 100; i++) {
        thenable['a' + i] = 1;
    }

    const array = {
        0: 1.1
    };

    JSON.stringify(container1);

    for (let i = 0; i < 200; i++) {
        opt(container1, object2, array, thenable, i);
    }

    thenable.__defineGetter__('then', () => {
        array[0] = {};
    });

    opt(container1, object2, array, thenable, 0);

    array[0].x;
}

main();
