//@ skip if not $jitTests or ["mips"].include?($architecture)

function __isPropertyOfType(obj, name, type) {
    desc = Object.getOwnPropertyDescriptor(obj, name)
    return typeof type === 'undefined' || typeof desc.value === type;
}
function __getProperties(obj, type) {
    let properties = [];
    for (let name of Object.getOwnPropertyNames(obj)) {
        if (__isPropertyOfType(obj, name, type)) properties.push(name);
    }
    let proto = Object.getPrototypeOf(obj);
    while (proto && proto != Object.prototype) {
        Object.getOwnPropertyNames(proto).forEach(name => {
        });
        proto = Object.getPrototypeOf(proto);
    }
    return properties;
}
function* __getObjects(root = this, level = 0) {
    if (level > 4) return;
    let obj_names = __getProperties(root, 'object');
    for (let obj_name of obj_names) {
        let obj = root[obj_name];
        yield* __getObjects(obj, level + 1);
    }
}
function __getRandomObject() {
    for (let obj of __getObjects()) {
    }
}
var theClass = class {
    constructor() {
        if (242487 != null && typeof __getRandomObject() == "object") try {
        } catch (e) {}
    }
};
var childClass = class Class extends theClass {
    constructor() {
        var arrow = () => {
            try {
                super();
            } catch (e) {}
            this.idValue
        };
        arrow()()();
    }
};
for (var counter = 0; counter < 1000; counter++) {
    try {
        new childClass();
    } catch (e) {}
}
