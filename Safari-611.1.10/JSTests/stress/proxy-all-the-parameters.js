const verbose = false;

function isPropertyOfType(obj, name, type) {
    let desc;
    desc = Object.getOwnPropertyDescriptor(obj, name)
    return typeof type === 'undefined' || typeof desc.value === type;
}

function getProperties(obj, type) {
    let properties = [];
    for (let name of Object.getOwnPropertyNames(obj)) {
        if (isPropertyOfType(obj, name, type))
            properties.push(name);
    }
    return properties;
}

function* generateObjects(root = this, level = 0) {
    if (level > 4)
        return;
    let obj_names = getProperties(root, 'object');
    for (let obj_name of obj_names) {
        if (obj_name.startsWith('$'))
            continue; // Ignore internal objects.
        let obj = root[obj_name];
        yield obj;
        yield* generateObjects(obj, level + 1);
    }
}

function getObjects() {
    let objects = [];
    for (let obj of generateObjects())
        if (!objects.includes(obj))
            objects.push(obj);
    return objects;
}

function getFunctions(obj) {
    return getProperties(obj, 'function');
}

const thrower = new Proxy({}, { get() { throw 0xc0defefe; } });

for (let o of getObjects()) {
    for (let f of getFunctions(o)) {
        const arityPlusOne = o[f].length + 1;
        if (verbose)
            print(`Calling ${o}['${f}'](${Array(arityPlusOne).fill("thrower")})`);
        try {
            o[f](Array(arityPlusOne).fill(thrower));
        } catch (e) {
            if (`${e}`.includes('constructor without new is invalid')) {
                try {
                    if (verbose)
                        print(`    Constructing instead`);
                    new o[f](Array(arityPlusOne).fill(thrower));
                } catch (e) {
                    if (verbose)
                        print(`    threw ${e}`);
                }
            } else {
                if (verbose)
                    print(`    threw ${e}`);
            }
        }
    }
}
