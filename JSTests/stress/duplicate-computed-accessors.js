function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

// Class methods.
(function () {
    var method1 = 'taste';
    var method2 = 'taste';

    class Cocoa {
        get [method1]() {
            return 'awesome';
        }

        get [method2]() {
            return 'great';
        }
    }

    var cocoa = new Cocoa();
    shouldBe(cocoa.taste, "great");
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        get [method1()]() {
            return 'awesome';
        }

        get [method2()]() {
            return 'great';
        }
    }

    var cocoa = new Cocoa();
    shouldBe(cocoa.taste, "great");
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        get [method1()]() {
            return this.value;
        }

        set [method2()](value) {
            this.value = value;
        }
    }

    var cocoa = new Cocoa();
    shouldBe(cocoa.taste, undefined);
    cocoa.taste = 'great';
    shouldBe(cocoa.taste, 'great');
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        get 'taste'() {
            return 'bad';
        }

        get [method1()]() {
            return this.value;
        }

        set [method2()](value) {
            this.value = value;
        }
    }

    var cocoa = new Cocoa();
    shouldBe(cocoa.taste, undefined);
    cocoa.taste = 'great';
    shouldBe(cocoa.taste, 'great');
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        get [method1()]() {
            return this.value;
        }

        set [method2()](value) {
            this.value = value;
        }

        get 'taste'() {
            return 'awesome';
        }

        set taste(value) {
        }
    }

    var cocoa = new Cocoa();
    shouldBe(cocoa.taste, 'awesome');
    cocoa.taste = 'great';
    shouldBe(cocoa.taste, 'awesome');
}());

// Class static methods.
(function () {
    var method1 = 'taste';
    var method2 = 'taste';

    class Cocoa {
        static get [method1]() {
            return 'awesome';
        }

        static get [method2]() {
            return 'great';
        }
    }

    shouldBe(Cocoa.taste, "great");
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        static get [method1()]() {
            return 'awesome';
        }

        static get [method2()]() {
            return 'great';
        }
    }

    shouldBe(Cocoa.taste, "great");
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        static get [method1()]() {
            return this.value;
        }

        static set [method2()](value) {
            this.value = value;
        }
    }

    shouldBe(Cocoa.taste, undefined);
    Cocoa.taste = 'great';
    shouldBe(Cocoa.taste, 'great');
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        static get 'taste'() {
            return 'bad';
        }

        static get [method1()]() {
            return this.value;
        }

        static set [method2()](value) {
            this.value = value;
        }
    }

    shouldBe(Cocoa.taste, undefined);
    Cocoa.taste = 'great';
    shouldBe(Cocoa.taste, 'great');
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    class Cocoa {
        static get [method1()]() {
            return this.value;
        }

        static set [method2()](value) {
            this.value = value;
        }

        static get 'taste'() {
            return 'awesome';
        }

        static set taste(value) {
        }
    }

    shouldBe(Cocoa.taste, 'awesome');
    Cocoa.taste = 'great';
    shouldBe(Cocoa.taste, 'awesome');
}());

// Object.
(function () {
    var method1 = 'taste';
    var method2 = 'taste';

    let Cocoa = {
        get [method1]() {
            return 'awesome';
        },

        get [method2]() {
            return 'great';
        }
    }

    shouldBe(Cocoa.taste, "great");
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    let Cocoa = {
        get [method1()]() {
            return 'awesome';
        },

        get [method2()]() {
            return 'great';
        }
    }

    shouldBe(Cocoa.taste, "great");
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    let Cocoa = {
        get [method1()]() {
            return this.value;
        },

        set [method2()](value) {
            this.value = value;
        }
    }

    shouldBe(Cocoa.taste, undefined);
    Cocoa.taste = 'great';
    shouldBe(Cocoa.taste, 'great');
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    let Cocoa = {
        get 'taste'() {
            return 'bad';
        },

        get [method1()]() {
            return this.value;
        },

        set [method2()](value) {
            this.value = value;
        }
    }

    shouldBe(Cocoa.taste, undefined);
    Cocoa.taste = 'great';
    shouldBe(Cocoa.taste, 'great');
}());

(function () {
    var counter = 0;
    function method1() {
        shouldBe(counter++, 0);
        return 'taste';
    }
    function method2() {
        shouldBe(counter++, 1);
        return 'taste';
    }

    let Cocoa = {
        get [method1()]() {
            return this.value;
        },

        set [method2()](value) {
            this.value = value;
        },

        get 'taste'() {
            return 'awesome';
        },

        set taste(value) {
        }
    }

    shouldBe(Cocoa.taste, 'awesome');
    Cocoa.taste = 'great';
    shouldBe(Cocoa.taste, 'awesome');
}());
