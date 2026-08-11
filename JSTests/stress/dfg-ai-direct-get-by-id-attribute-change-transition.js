function returnObject(object) {
    return object;
}

class Opt extends returnObject {
    p2 = 1;

    constructor(object) {
        object.p1;

        super(object);
    }
}

function createObject() {
    const object = {p1: 1};
    Object.defineProperty(object, 'p2', {
        configurable: true,
        enumerable: false,
        writable: true,
        value: 1
    });

    return object;
}

function getStructureID(object) {
    const desc = describe(object);

    return desc.match(/Structure 0x([a-f0-9]+)/)[1];
}

function main() {
    const a = createObject();
    new Opt(a);

    for (let i = 0; i < testLoopCount; i++) {
        new Opt(createObject());
    }

    const b = createObject();
    new Opt(b);

    const aStructureID = getStructureID(a);
    const bStructureID = getStructureID(b);

    if (aStructureID !== bStructureID)
        throw new Error('Structure ID mismatch.');
}

main();
