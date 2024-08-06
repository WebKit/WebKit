function main() {
    Function.prototype.__proto__ = new Proxy({}, {});

    const object = {
        nonConstructor() {

        }
    };

    function target() {

    }

    Function.prototype.__proto__ = Object.prototype;

    const nonConstructor = object.nonConstructor;

    nonConstructor.x = 1;
    target.x = 1;

    function returnArgument(a) {
        return a;
    }

    class A extends returnArgument {
        prototype = {};

        constructor(a) {
            super(a);
        }
    }

    new A(nonConstructor);
    new A(target);

    target.__defineGetter__('prototype', () => {

    });

    target.prototype.x = 1;
}

var errString = "<no error>";
try {
    main();
} catch (err) {
    errString = err.toString();
}

if (errString !== "TypeError: Attempting to change configurable attribute of unconfigurable property.")
    throw new Error(`Bad error: ${errString}`);
