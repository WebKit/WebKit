class PrivateMembersTestClassParent {
    instancePublicProperty = 'instancePublicPropertyValue parent';
    #instancePrivateProperty = 'instancePrivatePropertyValue parent';
    instancePublicMethod() { parent }
    #instancePrivateMethod() { parent }
    get instancePublicGetter() { parent }
    set instancePublicSetter(x) { parent }
    get instancePublicGetterSetter() { parent }
    set instancePublicGetterSetter(x) { parent }
    get #instancePrivateGetter() { parent }
    set #instancePrivateSetter(x) { parent }
    get #instancePrivateGetterSetter() { parent }
    set #instancePrivateGetterSetter(x) { parent }
    static classPublicProperty = 'classPublicPropertyValue parent';
    static #classPrivateProperty = 'classPrivatePropertyValue parent';
    static classPublicMethod() { parent }
    static #classPrivateMethod() { parent }
    static get classPublicGetter() { parent }
    static set classPublicSetter(x) { parent }
    static get classPublicGetterSetter() { parent }
    static set classPublicGetterSetter(x) { parent }
    static get #classPrivateGetter() { parent }
    static set #classPrivateSetter(x) { parent }
    static get #classPrivateGetterSetter() { parent }
    static set #classPrivateGetterSetter(x) { parent }
    parentInstancePublicProperty = 'parentInstancePublicPropertyValue';
    #parentInstancePrivateProperty = 'parentInstancePrivatePropertyValue';
    parentInstancePublicMethod() { }
    #parentInstancePrivateMethod() { }
    get parentInstancePublicGetter() { }
    set parentInstancePublicSetter(x) { }
    get parentInstancePublicGetterSetter() { }
    set parentInstancePublicGetterSetter(x) { }
    get #parentInstancePrivateGetter() { }
    set #parentInstancePrivateSetter(x) { }
    get #parentInstancePrivateGetterSetter() { }
    set #parentInstancePrivateGetterSetter(x) { }
    static parentClassPublicProperty = 'parentClassPublicPropertyValue';
    static #parentClassPrivateProperty = 'parentClassPrivatePropertyValue';
    static parentClassPublicMethod() { }
    static #parentClassPrivateMethod() { }
    static get parentClassPublicGetter() { }
    static set parentClassPublicSetter(x) { }
    static get parentClassPublicGetterSetter() { }
    static set parentClassPublicGetterSetter(x) { }
    static get #parentClassPrivateGetter() { }
    static set #parentClassPrivateSetter(x) { }
    static get #parentClassPrivateGetterSetter() { }
    static set #parentClassPrivateGetterSetter(x) { }
    static toString() { return "<redacted>"; }
}

class PrivateMembersTestClassChild extends PrivateMembersTestClassParent {
    instancePublicProperty = 'instancePublicPropertyValue child';
    #instancePrivateProperty = 'instancePrivatePropertyValue child';
    instancePublicMethod() { child }
    #instancePrivateMethod() { child }
    get instancePublicGetter() { child }
    set instancePublicSetter(x) { child }
    get instancePublicGetterSetter() { child }
    set instancePublicGetterSetter(x) { child }
    get #instancePrivateGetter() { child }
    set #instancePrivateSetter(x) { child }
    get #instancePrivateGetterSetter() { child }
    set #instancePrivateGetterSetter(x) { child }
    static classPublicProperty = 'classPublicPropertyValue child';
    static #classPrivateProperty = 'classPrivatePropertyValue child';
    static classPublicMethod() { child }
    static #classPrivateMethod() { child }
    static get classPublicGetter() { child }
    static set classPublicSetter(x) { child }
    static get classPublicGetterSetter() { child }
    static set classPublicGetterSetter(x) { child }
    static get #classPrivateGetter() { child }
    static set #classPrivateSetter(x) { child }
    static get #classPrivateGetterSetter() { child }
    static set #classPrivateGetterSetter(x) { child }
    childInstancePublicProperty = 'childInstancePublicPropertyValue';
    #childInstancePrivateProperty = 'childInstancePrivatePropertyValue';
    childInstancePublicMethod() { }
    #childInstancePrivateMethod() { }
    get childInstancePublicGetter() { }
    set childInstancePublicSetter(x) { }
    get childInstancePublicGetterSetter() { }
    set childInstancePublicGetterSetter(x) { }
    get #childInstancePrivateGetter() { }
    set #childInstancePrivateSetter(x) { }
    get #childInstancePrivateGetterSetter() { }
    set #childInstancePrivateGetterSetter(x) { }
    static childClassPublicProperty = 'childClassPublicPropertyValue';
    static #childClassPrivateProperty = 'childClassPrivatePropertyValue';
    static childClassPublicMethod() { }
    static #childClassPrivateMethod() { }
    static get childClassPublicGetter() { }
    static set childClassPublicSetter(x) { }
    static get childClassPublicGetterSetter() { }
    static set childClassPublicGetterSetter(x) { }
    static get #childClassPrivateGetter() { }
    static set #childClassPrivateSetter(x) { }
    static get #childClassPrivateGetterSetter() { }
    static set #childClassPrivateGetterSetter(x) { }
    static toString() { return "<redacted>"; }
}

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

    ProtocolTest.PropertyDescriptorUtilities.logForEach = function({name, value, get, set, ...extra}, index, array) {
        let maxPropertyNameLength = array.reduce((max, {name}) => name.length > max ? name.length : max, 0) + 2; // add 2 for the surrounding quotes
        let paddedName = JSON.stringify(name).padEnd(maxPropertyNameLength, " ");
        let descriptorKeys = [];
        for (let [key, value] of Object.entries(extra)) {
            if (value) {
                ProtocolTest.assert(typeof value === "boolean", `Property descriptor '${key}' has a non-boolean value '${JSON.stringify(value)}'.`);
                descriptorKeys.push(key);
            }
        }

        if (value)
            ProtocolTest.log(`    ${paddedName}  =>  ${ProtocolTest.PropertyDescriptorUtilities.stringifyRemoteObject(value)}  [${descriptorKeys.join(" | ")}]`);
        if (get)
            ProtocolTest.log(`    ${paddedName}  =>  get ${ProtocolTest.PropertyDescriptorUtilities.stringifyRemoteObject(get)}  [${descriptorKeys.join(" | ")}]`);
        if (set)
            ProtocolTest.log(`    ${paddedName}  =>  set ${ProtocolTest.PropertyDescriptorUtilities.stringifyRemoteObject(set)}  [${descriptorKeys.join(" | ")}]`);
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
