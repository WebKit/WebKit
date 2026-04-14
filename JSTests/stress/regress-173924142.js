//@ runDefault("--useConcurrentJIT=false", "--jitPolicyScale=0.01")

function assert(holds) {
    if (!holds)
        throw "Assertion failed";
}

const otherGlobal = createGlobalObject();
otherGlobal.eval(`returnMultipleValues = function(foobar) {
    return {
        ToBoolean: Boolean(foobar),
        TypeOfIsUndefined: typeof foobar === "undefined",
        TypeOfIsFunction: typeof foobar === "function",
        TypeOf: typeof foobar,
        CompareEq: foobar == null,
        LogicalNot: !foobar,
    };
}`);

function returnMultipleValues(foobar) {
    return [
        {
            ToBoolean: Boolean(foobar),
            TypeOfIsUndefined: typeof foobar === "undefined",
            TypeOfIsFunction: typeof foobar === "function",
            TypeOf: typeof foobar,
            CompareEq: foobar == null,
            LogicalNot: !foobar,
        },
        otherGlobal.returnMultipleValues(foobar)
    ]
}
noInline(returnMultipleValues)

const masquerader = makeMasquerader()
const otherGlobalMasquerader = otherGlobal.eval("makeMasquerader()")

let masqueraderBefore = returnMultipleValues(masquerader);
let otherGlobalMasqueraderBefore = returnMultipleValues(otherGlobalMasquerader);

for (var i = 0; i < 10000; i++) {
    returnMultipleValues(() => {})
}

let masqueraderAfter = returnMultipleValues(masquerader);
let otherGlobalMasqueraderAfter = returnMultipleValues(otherGlobalMasquerader);

for (const object of [masqueraderBefore, masqueraderAfter, otherGlobalMasqueraderBefore, otherGlobalMasqueraderAfter]) {
    for (const property of Object.getOwnPropertyNames(object[0])) {
        assert(object[0][property] !== object[1][property]);
    }
}
