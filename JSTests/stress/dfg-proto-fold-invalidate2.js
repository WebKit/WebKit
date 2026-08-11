//@ runDefault
function drain(msg) {
}
noInline(drain);

function opt(objectA, exitEarly) {
    if (exitEarly) {
        return;
    }

    objectA.tag;

    const arr = objectA.callee;

    arr[0] = 2.3023e-320;
}

function watchCalleeProperty(objectA) {
    return objectA.callee;
}

async function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function createObjectX(objectD) {
    const x = new Function();
    Reflect.setPrototypeOf(x, objectD);

    return x;
}

function createClonedArguments() {
    return createClonedArguments.arguments;
}

async function main() {
    createClonedArguments[0] = {};

    const objectE = {};
    const objectD = Object.create(objectE);
    const objectC = Object.create(objectD);
    const objectB = Object.create(objectC);
    const objectA = Object.create(objectB);

    const objectX = createClonedArguments();
    Object.setPrototypeOf(objectX, objectD);

    objectA.tag = 1;

    objectE.callee = {
        0: 1.1
    };

    for (let i = 0; i < 50; i++) {
        opt(objectA, /* exitEarly */ false);
    }

    for (let i = 0; i < 1000; i++) {
        watchCalleeProperty(objectE);
    }

    await sleep(1000);
    Reflect.setPrototypeOf(objectD, null);

    for (let i = 0; i < 1000; i++) {
        opt(objectA, /* exitEarly */ true);
    }

    await sleep(500);

    // Before: A -> B -> C -> D -> E
    // After:  A -> B -> X -> D -> E
    Reflect.setPrototypeOf(objectD, objectE);
    Reflect.setPrototypeOf(objectB, objectX);

    await sleep(1000);

    opt(objectA, /* exitEarly */ false);

    drain(createClonedArguments[0]);
}

main().catch(e => {
    print(e);
});

