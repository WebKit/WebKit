// Adding a property to an object with a (cacheable) dictionary structure does not
// transition the structure. So, the DFG must not prove the absence of 'then' based on
// a dictionary structure: neither CheckStructure nor a structure transition watchpoint
// can detect a 'then' property added after compilation.

function createDictionaryObject() {
    const object = { x: 42 };
    // Adding enough properties forces a transition to a cacheable dictionary structure.
    for (let i = 0; i < 1000; i++)
        object['p' + i] = i;
    return object;
}

let getterCalled = false;

function opt(object) {
    object.x;
    return Promise.resolve(object);
}
noInline(opt);

function main() {
    const object = createDictionaryObject();

    for (let i = 0; i < testLoopCount; i++)
        opt(object);

    // With concurrent JIT, the optimized code may not be installed yet. Keep running
    // until it is. numberOfDFGCompiles() returns a huge number when the DFG is
    // disabled, so this loop terminates in every configuration.
    for (let i = 0; i < 1e6 && numberOfDFGCompiles(opt) < 1; i++)
        opt(object);

    // This does not change object's structure because it is a dictionary structure.
    object.__defineGetter__('then', function () {
        getterCalled = true;
    });

    opt(object);

    if (!getterCalled)
        throw new Error("Promise.resolve() must observe the 'then' getter added after compilation");
}

main();
