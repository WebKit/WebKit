function test(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    var array = [];
    var unscopables = array[Symbol.unscopables];

    test(typeof unscopables, "object");
    test(unscopables.__proto__, undefined);
    test(String(Object.keys(unscopables).sort()), "at,copyWithin,entries,fill,find,findIndex,findLast,findLastIndex,flat,flatMap,group,groupToMap,includes,keys,toReversed,toSorted,toSpliced,values");
}());

(function () {
    var find = "Cocoa";
    var forEach = "Hidden";
    var array = [];
    test(typeof array.find, "function");

    with (array) {
        test(typeof find, "string");
        test(find, "Cocoa");
        test(typeof forEach, "function");
        test(__proto__, Array.prototype);  // array's unscopables.__proto__ is undefined => false.
    }
}());

(function () {
    var object = {
        [Symbol.unscopables]: {
            Cocoa: false,
            Cappuccino: true
        },

        Cocoa: null,
        Cappuccino: null
    };

    var Cocoa = "Cocoa";
    var Cappuccino = "Cappuccino";
    var toString = "toString";

    with (object) {
        test(Cocoa, null);
        test(Cappuccino, "Cappuccino");
        test(toString, "toString");  // object.toString is hidden by unscopables.toString.
    }

    object[Symbol.unscopables].Cocoa = true;

    with (object) {
        test(Cocoa, "Cocoa");
        test(Cappuccino, "Cappuccino");
    }
}());

(function () {
    var unscopables = Object.create(null);
    unscopables.Cocoa = false;
    unscopables.Cappuccino = true;

    var object = {
        [Symbol.unscopables]: unscopables,
        Cocoa: null,
        Cappuccino: null,
        Matcha: null
    };

    var Cocoa = "Cocoa";
    var Cappuccino = "Cappuccino";
    var Matcha = "Matcha";
    var toString = "toString";

    with (object) {
        test(Cocoa, null);
        test(Cappuccino, "Cappuccino");
        test(Matcha, null);
        test(toString, Object.prototype.toString);
    }

    object[Symbol.unscopables].Cocoa = true;
    object[Symbol.unscopables].Cappuccino = false;
    object[Symbol.unscopables].toString = true;

    with (object) {
        test(Cocoa, "Cocoa");
        test(Cappuccino, null);
        test(toString, "toString");
        test(Matcha, null);
    }
}());

(function () {
    var proto = Object.create(null);
    var unscopables = Object.create(proto);
    unscopables.Cocoa = false;
    unscopables.Cappuccino = true;

    var object = {
        [Symbol.unscopables]: unscopables,
        Cocoa: null,
        Cappuccino: null,
        Matcha: null
    };

    var Cocoa = "Cocoa";
    var Cappuccino = "Cappuccino";
    var Matcha = "Matcha";
    var toString = "toString";

    with (object) {
        test(Cocoa, null);
        test(Cappuccino, "Cappuccino");
        test(Matcha, null);
        test(toString, Object.prototype.toString);
    }

    object[Symbol.unscopables].Cocoa = true;
    object[Symbol.unscopables].Cappuccino = false;
    object[Symbol.unscopables].toString = true;

    with (object) {
        test(Cocoa, "Cocoa");
        test(Cappuccino, null);
        test(toString, "toString");
        test(Matcha, null);
    }

    proto.Matcha = true;

    with (object) {
        test(Cocoa, "Cocoa");
        test(Cappuccino, null);
        test(toString, "toString");
        test(Matcha, "Matcha");
    }
}());

(function () {
    var object = {
        get Cocoa() {
            throw new Error("bad trap");
        },
        Cappuccino: null
    };

    object[Symbol.unscopables] = {
        Cocoa: true,
        Cappuccino: true
    };

    var Cocoa = "Cocoa";
    var Cappuccino = "Cappuccino";
    var toString = "toString";

    with (object) {
        test(Cocoa, "Cocoa");
    }

    object[Symbol.unscopables].Cocoa = false;

    var error = null;
    try {
        with (object) {
            Cocoa;
        }
    } catch (e) {
        error = e;
    }
    test(String(error), "Error: bad trap");
}());

(function () {
    var object = {
        Cocoa: null,
    };
    Object.defineProperty(object, Symbol.unscopables, {
        get: function () {
            throw new Error("unscopables trap");
        }
    });

    var Cocoa = "Cocoa";
    var Cappuccino = "Cappuccino";

    var error = null;
    try {
        with (object) {
            Cocoa
        }
    } catch (e) {
        error = e;
    }
    test(String(error), "Error: unscopables trap");

    with (object) {
        test(Cappuccino, "Cappuccino");
    }
}());

(function () {
    var object = {
        [Symbol.unscopables]: {
            get Cocoa() {
                throw new Error("unscopables trap");
            }
        },
        Cocoa: null,
    };
    var Cocoa = "Cocoa";
    var Cappuccino = "Cappuccino";

    var error = null;
    try {
        with (object) {
            Cocoa
        }
    } catch (e) {
        error = e;
    }
    test(String(error), "Error: unscopables trap");

    with (object) {
        test(Cappuccino, "Cappuccino");
    }
}());

(function () {
    var object = {
        [Symbol.unscopables]: 42,
        Cocoa: "OK",
    };
    var Cocoa = "Cocoa";

    with (object) {
        test(Cocoa, "OK");
    }
}());
