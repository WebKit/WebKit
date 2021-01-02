function shouldBe(actual, expected, testInfo) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual} (${testInfo})`);
}

let caller = null;
function updateCaller() {
    caller = updateCaller.caller;
}
noInline(updateCaller);

function normalStrictFunction() { "use strict"; updateCaller(); }

const { get, set } = Object.getOwnPropertyDescriptor({
    get accessor() { updateCaller(); },
    set accessor(_v) { updateCaller(); },
}, "accessor");

const arrowFunction = () => { updateCaller(); };
const asyncArrowFunction = async () => { updateCaller(); };

const functionsHiddenAsCallers = [
    normalStrictFunction,
    normalStrictFunction.bind(),
    get,
    set,
    arrowFunction,
    asyncArrowFunction,
    function* syncGenerator() { updateCaller(); },
    { * syncGeneratorMethod() { updateCaller(); } }.syncGeneratorMethod,
    { method() { updateCaller(); } }.method,
    async function asyncFunction() { updateCaller(); },
    { async asyncMethod() { updateCaller(); } }.asyncMethod,
    async function* asyncGenerator() { updateCaller(); },
    { async * asyncGeneratorMethod() { updateCaller(); } }.asyncGeneratorMethod,
];

const constructorsHiddenAsCallers = [
    class baseConstructor {
        constructor() { updateCaller(); }
    },
    class derivedConstructor extends Array {
        constructor() { super(); updateCaller(); }
    },
];

(function hiddenAsCallers() {
    for (const fn of functionsHiddenAsCallers) {
        for (let i = 0; i < 1e4; ++i) {
            caller = null;
            fn();
            shouldBe(caller, null, fn.name);
        }
    }

    for (const C of constructorsHiddenAsCallers) {
        for (let i = 0; i < 1e4; ++i) {
            caller = null;
            new C();
            shouldBe(caller, null, C.name);
        }
    }
})();
