function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

(function() {
    function tryToLeakThisViaGetById() {
        class Leaker {
            leak() {
                return super.foo;
            }
        }

        Leaker.prototype.__proto__ = new Proxy({}, {
            get(target, propertyName, receiver) {
                return receiver;
            }
        });

        const foo = 42;
        const {leak} = Leaker.prototype;

        return (() => leak())();
    }

    function tryToLeakThisViaGetByVal() {
        class Leaker {
            leak() {
                return super[Math.random() < 0.5 ? "foo" : "bar"];
            }
        }

        Leaker.prototype.__proto__ = new Proxy({}, {
            get(target, propertyName, receiver) {
                return receiver;
            }
        });

        const foo = 42;
        const bar = 84;
        const {leak} = Leaker.prototype;

        return (() => leak())();
    }

    function tryToLeakThisViaSetById() {
        let receiver;
        class Leaker {
            leak() {
                super.foo = {};
                return receiver;
            }
        }
        Leaker.prototype.__proto__ = new Proxy({}, {
            set(target, propertyName, value, __receiver) {
                receiver = __receiver;
                return true;
            }
        });

        const foo = 42;
        const {leak} = Leaker.prototype;

        return (() => leak())();
    }

    function tryToLeakThisViaSetByVal() {
        let receiver;
        class Leaker {
            leak() {
                super[Math.random() < 0.5 ? "foo" : "bar"] = {};
                return receiver;
            }
        }

        Leaker.prototype.__proto__ = new Proxy({}, {
            set(target, propertyName, value, __receiver) {
                receiver = __receiver;
                return true;
            }
        });

        const foo = 42;
        const bar = 84;
        const {leak} = Leaker.prototype;

        return (() => leak())();
    }

    for (var i = 0; i < 1e5; i++) {
        assert(tryToLeakThisViaGetById() === undefined);
        assert(tryToLeakThisViaGetByVal() === undefined);
        assert(tryToLeakThisViaSetById() === undefined);
        assert(tryToLeakThisViaSetByVal() === undefined);
    }
})();
