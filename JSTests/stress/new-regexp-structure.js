function assert(condition) {
    if (!condition)
        throw new Error("Assertion failed");
}

function getStructureID(object) {
    const desc = describe(object);
    const match = desc.match(/Structure 0x([a-f0-9]+)/);
    assert(match);
    return match[1];
}

let global2 = createGlobalObject();

function doLoopRegExp() {
    let pattern = "A";
    let regExp;
    for (let i = 0; i < testLoopCount; i++)
        regExp = new global2.RegExp(pattern, "g");
    return regExp;
}

function testRegExpStructure() {
    const pattern = "A";

    let expectedRegExp = new global2.RegExp(pattern, "g");
    let loopRegExp = doLoopRegExp();

    let loopStructure = getStructureID(loopRegExp);
    let expectedStructure = getStructureID(expectedRegExp);
    assert(loopStructure === expectedStructure);

    assert(expectedRegExp instanceof global2.RegExp);
    assert(loopRegExp instanceof global2.RegExp);
    assert(!(expectedRegExp instanceof RegExp));
    assert(!(loopRegExp instanceof RegExp));
}

function testRegExpProperty() {
    const pattern = "B";
    let r = 0;
    for (let i = 0; i < testLoopCount; i++) {
        r = new global2.RegExp(pattern, "g");
        for (let i = 0; i < 10; i++)
            r.b = true;
    }
    return r;
}

testRegExpStructure();
testRegExpProperty();