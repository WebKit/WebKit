function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function revivedObjProxy(key, value) {
    if (key === 'a') {
        let {proxy, revoke} = Proxy.revocable({}, {});
        revoke();
        this.b = proxy;
    }

    return value;
}

function revivedArrProxy(key, value) {
    if (key === '0') {
        let {proxy, revoke} = Proxy.revocable([], {});
        revoke();
        this[1] = proxy;
    }

    return value;
}

const objJSON = '{"a": 1, "b": 2}';
const arrJSON = '[3, 4]';
const isArrayError = 'TypeError: Array.isArray cannot be called on a Proxy that has been revoked';

for (let i = 1; i < 10000; i++) {
    let error;
    try {
        JSON.parse(objJSON, revivedObjProxy);
    } catch (e) {
        error = e;
    }
    shouldBe(error.toString(), isArrayError);
}

for (let i = 1; i < 10000; i++) {
    let error;
    try {
        JSON.parse(arrJSON, revivedArrProxy);
    } catch (e) {
        error = e;
    }
    shouldBe(error.toString(), isArrayError);
}
