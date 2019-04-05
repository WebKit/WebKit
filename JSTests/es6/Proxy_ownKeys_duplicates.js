function handler(key) {
    return {
        getOwnPropertyDescriptor(t, n) {
            // Required to prevent Object.keys() from discarding results
            return {
                enumerable: true,
                configurable: true,
            };
        },
        ownKeys(t) {
            return [key, key];
        }
    };
}

function shouldThrow(op, errorConstructor, desc) {
    try {
        op();
    } catch (e) {
        if (!(e instanceof errorConstructor)) {
            throw new Error(`threw ${e}, but should have thrown ${errorConstructor.name}`);
        }
        return;
    }
    throw new Error(`Expected ${desc || 'operation'} to throw ${errorConstructor.name}, but no exception thrown`);
}

function test() {

var symbol = Symbol("test");
var proxyNamed = new Proxy({}, handler("A"));
var proxyIndexed = new Proxy({}, handler(0));
var proxySymbol = new Proxy({}, handler(symbol));

shouldThrow(() => Object.keys(proxyNamed), TypeError, "Object.keys with duplicate named properties");
shouldThrow(() => Object.keys(proxyIndexed), TypeError, "Object.keys with duplicate indexed properties");
shouldThrow(() => Object.keys(proxySymbol), TypeError, "Object.keys with duplicate symbol properties");

shouldThrow(() => Object.getOwnPropertyNames(proxyNamed), TypeError, "Object.getOwnPropertyNames with duplicate named properties");
shouldThrow(() => Object.getOwnPropertyNames(proxyIndexed), TypeError, "Object.getOwnPropertyNames with duplicate indexed properties");
shouldThrow(() => Object.getOwnPropertyNames(proxySymbol), TypeError, "Object.getOwnPropertyNames with duplicate symbol properties");

shouldThrow(() => Object.getOwnPropertySymbols(proxyNamed), TypeError, "Object.getOwnPropertySymbols with duplicate named properties");
shouldThrow(() => Object.getOwnPropertySymbols(proxyIndexed), TypeError, "Object.getOwnPropertySymbols with duplicate indexed properties");
shouldThrow(() => Object.getOwnPropertySymbols(proxySymbol), TypeError, "Object.getOwnPropertySymbols with duplicate symbol properties");

return true;

}

if (!test())
    throw new Error("Test failed");

