function createClassHierarchy(depth) {
    let currentClass = class Base { };
    for (let i = 0; i < depth; i++) {
        currentClass = class extends currentClass {};
    }
    return currentClass;
}

let ctor = createClassHierarchy(10);
let start = Date.now();
for (let i = 0; i < 500000; i++) {
    let x = new ctor({}, {}, 20, 30, 40, 50, 60, {}, 80, true, false);
}

const verbose = false;
if (verbose)
    print(Date.now() - start);
