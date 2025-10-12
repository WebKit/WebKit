function opt(access, instance, transition) {
    let result;
    instance.x;

    for (let i = 0; i < 2; i++) {
        transition.y = 1;

        if (access) {
            result = instance.exports;
        }
    }

    return result;
}

function main() {
    const emptyModule = new WebAssembly.Module(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    const instance = new WebAssembly.Instance(emptyModule, {});
    instance.x = 1;

    const object = {x: 1, p1: 1, p2: 1, p3: 1, p4: 0x1234};

    for (let i = 0; i < testLoopCount; i++) {
        opt(/* access */ true, instance, {});
        opt(/* access */ false, object, {});
    }
}

main();
