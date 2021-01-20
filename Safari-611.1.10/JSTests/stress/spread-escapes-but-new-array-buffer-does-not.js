function assert(b) {
    if (!b)
        throw new Error;
}
noInline(assert);

function getProperties(obj) {
    let properties = [];
    for (let name of Object.getOwnPropertyNames(obj)) {
        properties.push(name);
    }
    return properties;
}

function theFunc(obj, index) {
    let args = [42, 20];
    let functions = getProperties(obj);
    let func = functions[index % functions.length];
    obj[func](...args);
}

let obj = {
    valueOf: function (x, y) {
        assert(x === 42);
        assert(y === 20);
        try {
        } catch (e) {}
    }
};

for (let i = 0; i < 1e5; ++i) {
    for (let _i = 0; _i < 100; _i++) {
    }
    theFunc(obj, 897989);
}
