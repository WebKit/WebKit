function makeArray(length) {
    return Array(length).fill().map((item, i) => {
        let code = (i % 2) ? ("Z".charCodeAt(0) - i) : ("A".charCodeAt(0) + i);
        return String.fromCharCode(code);
    });
}

function makeObject(length) {
    // Intentionally unsorted to demonstrate how `getProperties` fetches properties in the order they were added.
    return makeArray(length).reduce((accumulator, item, i) => {
        accumulator[item] = i;
        return accumulator;
    }, {});
}

function makeMap(length) {
    return new Map(makeArray(length).map((item, i) => [item, i]));
}

function makeSet(length) {
    return new Set(makeArray(length));
}

function makeWeakMap(length) {
    return new WeakMap(makeArray(length).map((item, i) => [new String(item), i]));
}

function makeWeakSet(length) {
    return new WeakSet(makeArray(length).map((item) => new String(item)));
}

TestPage.registerInitializer(() => {
    ProtocolTest.PropertyDescriptorUtilities = {};

    ProtocolTest.PropertyDescriptorUtilities.logForEach = function({name, value, ...extra}, index, array) {
        let maxPropertyNameLength = array.reduce((max, {name}) => name.length > max ? name.length : max, 0) + 2; // add 2 for the surrounding quotes
        let paddedName = JSON.stringify(name).padEnd(maxPropertyNameLength, " ");
        let descriptorKeys = [];
        for (let [key, value] of Object.entries(extra)) {
            if (value) {
                ProtocolTest.assert(typeof value === "boolean", `Property descriptor '${key}' has a non-boolean value '${value}'.`);
                descriptorKeys.push(key);
            }
        }

        ProtocolTest.log(`    ${paddedName}  =>  ${ProtocolTest.PropertyDescriptorUtilities.stringifyRemoteObject(value)}  [${descriptorKeys.join(" | ")}]`);
    };

    ProtocolTest.PropertyDescriptorUtilities.stringifyRemoteObject = function(remoteObject) {
        let result = "";
        if (remoteObject) {
            result += ("value" in remoteObject) ? JSON.stringify(remoteObject.value) : JSON.stringify(remoteObject.description);
            result += " (" + remoteObject.type;
            if (remoteObject.subtype)
                result += " " + remoteObject.subtype
            result += ")";
        }
        return result;
    };
});
