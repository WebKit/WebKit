function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error('Not thrown!');
}

const obj = {};
const s = [0.1].toLocaleString().padEnd(2 ** 31 - 1, "ab");

Object.defineProperty(obj, "foo", {value: s, writable: false, enumerable: true, configurable: false});

shouldThrow(() => {
    Object.defineProperty(obj, "foo", {value: "bar"});
}, "TypeError: Attempting to change value of a readonly property.");
