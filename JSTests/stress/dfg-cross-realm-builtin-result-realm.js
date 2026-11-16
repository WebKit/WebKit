// Values created by another realm's built-ins must come from that realm, also from optimized callers.

const other = createGlobalObject();
const OtherBoolean = other.Boolean;
const OtherObject = other.Object;
const OtherMap = other.Map;
const OtherSet = other.Set;
const OtherWeakMap = other.WeakMap;
const OtherWeakSet = other.WeakSet;
const masquerader = other.makeMasquerader();

// Objects that masquerade as undefined are only falsy in their own realm.
function booleanCall(x) { return OtherBoolean(x); }
noInline(booleanCall);
function booleanClosureCall(f, x) { return f(x); }
noInline(booleanClosureCall);
function objectCall(x) { return OtherObject(x); }
noInline(objectCall);
function objectConstruct() { return new OtherObject(); }
noInline(objectConstruct);
function mapConstruct() { return new OtherMap(); }
noInline(mapConstruct);
function setConstruct() { return new OtherSet(); }
noInline(setConstruct);
function weakMapConstruct() { return new OtherWeakMap(); }
noInline(weakMapConstruct);
function weakSetConstruct() { return new OtherWeakSet(); }
noInline(weakSetConstruct);
Object.prototype.otherExec = other.RegExp.prototype.exec;
const regExp = /a/;
function regExpExec(x) { return x.otherExec("a"); }
noInline(regExpExec);

// [label, test returning whether the result is right]
const tests = [
    ["other.Boolean(masquerader)", () => booleanCall(masquerader) === false],
    ["Boolean / other.Boolean closure call (masquerader)", i => booleanClosureCall(i & 1 ? Boolean : OtherBoolean, masquerader) === Boolean(i & 1)],
    ["other.Object(1)", () => Object.getPrototypeOf(objectCall(1)) === other.Number.prototype],
    ["new other.Object()", () => Object.getPrototypeOf(objectConstruct()) === other.Object.prototype],
    ["new other.Map()", () => Object.getPrototypeOf(mapConstruct()) === other.Map.prototype],
    ["new other.Set()", () => Object.getPrototypeOf(setConstruct()) === other.Set.prototype],
    ["new other.WeakMap()", () => Object.getPrototypeOf(weakMapConstruct()) === other.WeakMap.prototype],
    ["new other.WeakSet()", () => Object.getPrototypeOf(weakSetConstruct()) === other.WeakSet.prototype],
    ["other.RegExp.prototype.exec result", () => Object.getPrototypeOf(regExpExec(regExp)) === other.Array.prototype],
];

const failures = new Map();
for (let i = 0; i < testLoopCount; ++i) {
    for (const [label, test] of tests) {
        if (!failures.has(label) && !test(i))
            failures.set(label, `${label}: wrong realm at iteration ${i}`);
    }
}

if (failures.size)
    throw new Error([...failures.values()].join("\n"));
