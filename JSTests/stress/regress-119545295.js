function main() {
    const new_target = (function () { }).bind();

    Object.defineProperty(new_target, 'prototype', {
        get() {
            // Make the global object have a bad time during construct.
            Object.setPrototypeOf(Array.prototype, new Proxy({}, {
                get() {
                    return 3;
                }
            }));
        }
    });

    const array = Reflect.construct(Array, [1.1, 2.2, 3.3], new_target);
    delete array[0];
    array[0];

    if ($vm.indexingMode(array) != "ArrayWithSlowPutArrayStorage")
        throw Error("Unexpected indexing mode")
}

main();
