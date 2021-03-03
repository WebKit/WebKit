function shouldThrow(func, expectedMessage, testInfo) {
    var errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== expectedMessage)
            throw new Error(`Bad error: ${error}\n${testInfo}`)
    }
    if (!errorThrown)
        throw new Error(`Didn't throw!\n${testInfo}`);
}

var get = function() {};
var set = function(_v) {};

var testCases = [
    {
        targetDescriptor: { value: 1, writable: false, enumerable: true, configurable: false },
        resultDescriptor: {           writable: false, enumerable: true, configurable: false },
    },
    {
        targetDescriptor: { value: 1, writable: false, enumerable: true, configurable: false },
        resultDescriptor: {                            enumerable: true, configurable: false },
    },
    {
        targetDescriptor: { get: undefined, set: undefined, enumerable: true, configurable: false },
        resultDescriptor: {                                 enumerable: true, configurable: false },
    },
    {
        targetDescriptor: { get: get, set: set, enumerable: true, configurable: false },
        resultDescriptor: { get: get,           enumerable: true, configurable: false },
    },
    {
        targetDescriptor: { get: get, set: undefined, enumerable: true, configurable: false },
        resultDescriptor: {           set: undefined, enumerable: true, configurable: false },
    },
    {
        targetDescriptor: { get: get, set: set, enumerable: true, configurable: false },
        resultDescriptor: {                     enumerable: true, configurable: false },
    },
    {
        targetDescriptor: { value: 1, writable: false, enumerable: true, configurable: false },
        resultDescriptor: { value: 1, writable: false,                   configurable: false },
    },
    {
        targetDescriptor: { value: 1, writable: false, enumerable: true, configurable: false },
        resultDescriptor: { value: 1,                                    configurable: false },
    },
    {
        targetDescriptor: { value: undefined, writable: false, enumerable: true, configurable: false },
        resultDescriptor: {                   writable: false,                   configurable: false },
    },
    {
        targetDescriptor: { value: undefined, writable: false, enumerable: true, configurable: false },
        resultDescriptor: {                                                      configurable: false },
    },
];

testCases.forEach(function(testCase) {
    var target = {};
    Object.defineProperty(target, "foo", testCase.targetDescriptor);

    var proxy = new Proxy(target, {
        getOwnPropertyDescriptor: function() {
            return testCase.resultDescriptor;
        },
    });

    shouldThrow(
        () => { Object.getOwnPropertyDescriptor(proxy, "foo"); },
        "TypeError: Result from 'getOwnPropertyDescriptor' fails the IsCompatiblePropertyDescriptor test",
        JSON.stringify(testCase, replacer, 2).replace(/"/g, "")
    );
});

function replacer(_key, value) {
    var type = typeof value;
    return type === "undefined" || type === "function" ? type : value;
}
