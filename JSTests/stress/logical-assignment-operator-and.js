function shouldBe(func, expected) {
    let actual = func();
    if (typeof expected === "function" ? !(actual instanceof expected) : actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name} but saw ${error && error.name}!`);
}

function shouldThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if (!(error instanceof SyntaxError))
        throw new Error(`Expected SyntaxError but saw ${error && error.name}!`);
}

shouldBe(function() {
    let x;
    return x &&= 42;
}, undefined);

shouldBe(function() {
    let x;
    x &&= 42;
    return x;
}, undefined);

shouldBe(function() {
    let x = undefined;
    return x &&= 42;
}, undefined);

shouldBe(function() {
    let x = undefined;
    x &&= 42;
    return x;
}, undefined);

shouldBe(function() {
    let x = null;
    return x &&= 42;
}, null);

shouldBe(function() {
    let x = null;
    x &&= 42;
    return x;
}, null);

shouldBe(function() {
    let x = true;
    return x &&= 42;
}, 42);

shouldBe(function() {
    let x = true;
    x &&= 42;
    return x;
}, 42);

shouldBe(function() {
    let x = false;
    return x &&= 42;
}, false);

shouldBe(function() {
    let x = false;
    x &&= 42;
    return x;
}, false);

shouldBe(function() {
    let x = 0;
    return x &&= 42;
}, 0);

shouldBe(function() {
    let x = 0;
    x &&= 42;
    return x;
}, 0);

shouldBe(function() {
    let x = 1;
    return x &&= 42;
}, 42);

shouldBe(function() {
    let x = 1;
    x &&= 42;
    return x;
}, 42);

shouldBe(function() {
    let x = "";
    return x &&= 42;
}, "");

shouldBe(function() {
    let x = "";
    x &&= 42;
    return x;
}, "");

shouldBe(function() {
    let x = "test";
    return x &&= 42;
}, 42);

shouldBe(function() {
    let x = "test";
    x &&= 42;
    return x;
}, 42);

shouldBe(function() {
    let x = {};
    return x &&= 42;
}, 42);

shouldBe(function() {
    let x = {};
    x &&= 42;
    return x;
}, 42);

shouldBe(function() {
    let x = [];
    return x &&= 42;
}, 42);

shouldBe(function() {
    let x = [];
    x &&= 42;
    return x;
}, 42);



shouldBe(function() {
    const x = undefined;
    return x &&= 42;
}, undefined);

shouldBe(function() {
    const x = undefined;
    x &&= 42;
    return x;
}, undefined);

shouldBe(function() {
    const x = null;
    return x &&= 42;
}, null);

shouldBe(function() {
    const x = null;
    x &&= 42;
    return x;
}, null);

shouldThrow(function() {
    const x = true;
    return x &&= 42;
}, TypeError);

shouldBe(function() {
    const x = false;
    return x &&= 42;
}, false);

shouldBe(function() {
    const x = false;
    x &&= 42;
    return x;
}, false);

shouldBe(function() {
    const x = 0;
    return x &&= 42;
}, 0);

shouldBe(function() {
    const x = 0;
    x &&= 42;
    return x;
}, 0);

shouldThrow(function() {
    const x = 1;
    return x &&= 42;
}, TypeError);

shouldBe(function() {
    const x = "";
    return x &&= 42;
}, "");

shouldBe(function() {
    const x = "";
    x &&= 42;
    return x;
}, "");

shouldThrow(function() {
    const x = "test";
    return x &&= 42;
}, TypeError);

shouldThrow(function() {
    const x = {};
    return x &&= 42;
}, TypeError);

shouldThrow(function() {
    const x = [];
    return x &&= 42;
}, TypeError);



shouldBe(function() {
    let x = {};
    return x.a &&= 42;
}, undefined);

shouldBe(function() {
    let x = {};
    x.a &&= 42;
    return x.a;
}, undefined);

shouldBe(function() {
    let x = {a: undefined};
    return x.a &&= 42;
}, undefined);

shouldBe(function() {
    let x = {a: undefined};
    x.a &&= 42;
    return x.a;
}, undefined);

shouldBe(function() {
    let x = {a: null};
    return x.a &&= 42;
}, null);

shouldBe(function() {
    let x = {a: null};
    x.a &&= 42;
    return x.a;
}, null);

shouldBe(function() {
    let x = {a: true};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: true};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    let x = {a: false};
    return x.a &&= 42;
}, false);

shouldBe(function() {
    let x = {a: false};
    x.a &&= 42;
    return x.a;
}, false);

shouldBe(function() {
    let x = {a: 0};
    return x.a &&= 42;
}, 0);

shouldBe(function() {
    let x = {a: 0};
    x.a &&= 42;
    return x.a;
}, 0);

shouldBe(function() {
    let x = {a: 1};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: 1};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    let x = {a: ""};
    return x.a &&= 42;
}, "");

shouldBe(function() {
    let x = {a: ""};
    x.a &&= 42;
    return x.a;
}, "");

shouldBe(function() {
    let x = {a: "test"};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: "test"};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    let x = {a: {}};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: {}};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    let x = {a: []};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: []};
    x.a &&= 42;
    return x.a;
}, 42);



shouldBe(function() {
    const x = {};
    return x.a &&= 42;
}, undefined);

shouldBe(function() {
    const x = {};
    x.a &&= 42;
    return x.a;
}, undefined);

shouldBe(function() {
    const x = {a: undefined};
    return x.a &&= 42;
}, undefined);

shouldBe(function() {
    const x = {a: undefined};
    x.a &&= 42;
    return x.a;
}, undefined);

shouldBe(function() {
    const x = {a: null};
    return x.a &&= 42;
}, null);

shouldBe(function() {
    const x = {a: null};
    x.a &&= 42;
    return x.a;
}, null);

shouldBe(function() {
    const x = {a: true};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: true};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    const x = {a: false};
    return x.a &&= 42;
}, false);

shouldBe(function() {
    const x = {a: false};
    x.a &&= 42;
    return x.a;
}, false);

shouldBe(function() {
    const x = {a: 0};
    return x.a &&= 42;
}, 0);

shouldBe(function() {
    const x = {a: 0};
    x.a &&= 42;
    return x.a;
}, 0);

shouldBe(function() {
    const x = {a: 1};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: 1};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    const x = {a: ""};
    return x.a &&= 42;
}, "");

shouldBe(function() {
    const x = {a: ""};
    x.a &&= 42;
    return x.a;
}, "");

shouldBe(function() {
    const x = {a: "test"};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: "test"};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    const x = {a: {}};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: {}};
    x.a &&= 42;
    return x.a;
}, 42);

shouldBe(function() {
    const x = {a: []};
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: []};
    x.a &&= 42;
    return x.a;
}, 42);



shouldBe(function() {
    let x = {};
    return x["a"] &&= 42;
}, undefined);

shouldBe(function() {
    let x = {};
    x["a"] &&= 42;
    return x["a"];
}, undefined);

shouldBe(function() {
    let x = {a: undefined};
    return x["a"] &&= 42;
}, undefined);

shouldBe(function() {
    let x = {a: undefined};
    x["a"] &&= 42;
    return x["a"];
}, undefined);

shouldBe(function() {
    let x = {a: null};
    return x["a"] &&= 42;
}, null);

shouldBe(function() {
    let x = {a: null};
    x["a"] &&= 42;
    return x["a"];
}, null);

shouldBe(function() {
    let x = {a: true};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: true};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    let x = {a: false};
    return x["a"] &&= 42;
}, false);

shouldBe(function() {
    let x = {a: false};
    x["a"] &&= 42;
    return x["a"];
}, false);

shouldBe(function() {
    let x = {a: 0};
    return x["a"] &&= 42;
}, 0);

shouldBe(function() {
    let x = {a: 0};
    x["a"] &&= 42;
    return x["a"];
}, 0);

shouldBe(function() {
    let x = {a: 1};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: 1};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    let x = {a: ""};
    return x["a"] &&= 42;
}, "");

shouldBe(function() {
    let x = {a: ""};
    x["a"] &&= 42;
    return x["a"];
}, "");

shouldBe(function() {
    let x = {a: "test"};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: "test"};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    let x = {a: {}};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: {}};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    let x = {a: []};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    let x = {a: []};
    x["a"] &&= 42;
    return x["a"];
}, 42);



shouldBe(function() {
    const x = {};
    return x["a"] &&= 42;
}, undefined);

shouldBe(function() {
    const x = {};
    x["a"] &&= 42;
    return x["a"];
}, undefined);

shouldBe(function() {
    const x = {a: undefined};
    return x["a"] &&= 42;
}, undefined);

shouldBe(function() {
    const x = {a: undefined};
    x["a"] &&= 42;
    return x["a"];
}, undefined);

shouldBe(function() {
    const x = {a: null};
    return x["a"] &&= 42;
}, null);

shouldBe(function() {
    const x = {a: null};
    x["a"] &&= 42;
    return x["a"];
}, null);

shouldBe(function() {
    const x = {a: true};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: true};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    const x = {a: false};
    return x["a"] &&= 42;
}, false);

shouldBe(function() {
    const x = {a: false};
    x["a"] &&= 42;
    return x["a"];
}, false);

shouldBe(function() {
    const x = {a: 0};
    return x["a"] &&= 42;
}, 0);

shouldBe(function() {
    const x = {a: 0};
    x["a"] &&= 42;
    return x["a"];
}, 0);

shouldBe(function() {
    const x = {a: 1};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: 1};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    const x = {a: ""};
    return x["a"] &&= 42;
}, "");

shouldBe(function() {
    const x = {a: ""};
    x["a"] &&= 42;
    return x["a"];
}, "");

shouldBe(function() {
    const x = {a: "test"};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: "test"};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    const x = {a: {}};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: {}};
    x["a"] &&= 42;
    return x["a"];
}, 42);

shouldBe(function() {
    const x = {a: []};
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    const x = {a: []};
    x["a"] &&= 42;
    return x["a"];
}, 42);



shouldBe(function() {
    let x = [];
    return x[0] &&= 42;
}, undefined);

shouldBe(function() {
    let x = [];
    x[0] &&= 42;
    return x[0];
}, undefined);

shouldBe(function() {
    let x = [undefined];
    return x[0] &&= 42;
}, undefined);

shouldBe(function() {
    let x = [undefined];
    x[0] &&= 42;
    return x[0];
}, undefined);

shouldBe(function() {
    let x = [null];
    return x[0] &&= 42;
}, null);

shouldBe(function() {
    let x = [null];
    x[0] &&= 42;
    return x[0];
}, null);

shouldBe(function() {
    let x = [true];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    let x = [true];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    let x = [false];
    return x[0] &&= 42;
}, false);

shouldBe(function() {
    let x = [false];
    x[0] &&= 42;
    return x[0];
}, false);

shouldBe(function() {
    let x = [0];
    return x[0] &&= 42;
}, 0);

shouldBe(function() {
    let x = [0];
    x[0] &&= 42;
    return x[0];
}, 0);

shouldBe(function() {
    let x = [1];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    let x = [1];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    let x = [""];
    return x[0] &&= 42;
}, "");

shouldBe(function() {
    let x = [""];
    x[0] &&= 42;
    return x[0];
}, "");

shouldBe(function() {
    let x = ["test"];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    let x = ["test"];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    let x = [{}];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    let x = [{}];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    let x = [[]];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    let x = [[]];
    x[0] &&= 42;
    return x[0];
}, 42);



shouldBe(function() {
    const x = [];
    return x[0] &&= 42;
}, undefined);

shouldBe(function() {
    const x = [];
    x[0] &&= 42;
    return x[0];
}, undefined);

shouldBe(function() {
    const x = [undefined];
    return x[0] &&= 42;
}, undefined);

shouldBe(function() {
    const x = [undefined];
    x[0] &&= 42;
    return x[0];
}, undefined);

shouldBe(function() {
    const x = [null];
    return x[0] &&= 42;
}, null);

shouldBe(function() {
    const x = [null];
    x[0] &&= 42;
    return x[0];
}, null);

shouldBe(function() {
    const x = [true];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    const x = [true];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    const x = [false];
    return x[0] &&= 42;
}, false);

shouldBe(function() {
    const x = [false];
    x[0] &&= 42;
    return x[0];
}, false);

shouldBe(function() {
    const x = [0];
    return x[0] &&= 42;
}, 0);

shouldBe(function() {
    const x = [0];
    x[0] &&= 42;
    return x[0];
}, 0);

shouldBe(function() {
    const x = [1];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    const x = [1];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    const x = [""];
    return x[0] &&= 42;
}, "");

shouldBe(function() {
    const x = [""];
    x[0] &&= 42;
    return x[0];
}, "");

shouldBe(function() {
    const x = ["test"];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    const x = ["test"];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    const x = [{}];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    const x = [{}];
    x[0] &&= 42;
    return x[0];
}, 42);

shouldBe(function() {
    const x = [[]];
    return x[0] &&= 42;
}, 42);

shouldBe(function() {
    const x = [[]];
    x[0] &&= 42;
    return x[0];
}, 42);



shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y + z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y = z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y && z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y ?? z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y || z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y &&= z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y ??= z;
}, false);

shouldBe(function() {
    let x = false;
    let y = 1;
    let z = 2;
    return x &&= y ||= z;
}, false);



shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y + z;
}, 3);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y = z;
}, 2);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y && z;
}, 2);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y ?? z;
}, 1);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y || z;
}, 1);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y &&= z;
}, 2);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y ??= z;
}, 1);

shouldBe(function() {
    let x = true;
    let y = 1;
    let z = 2;
    return x &&= y ||= z;
}, 1);



shouldBe(function() {
    let log = [];

    let a = true;
    let x = {
        get a() {
            log.push("get");
            return a;
        },
        set a(v) {
            log.push("set");
            a = v;
        },
    };

    x.a &&= 42;
    x.a &&= 42;

    return log.join(" ") + " " + a;
}, "get set get set 42");

shouldBe(function() {
    let log = [];

    let a = false;
    let x = {
        get a() {
            log.push("get");
            return a;
        },
        set a(v) {
            log.push("set");
            a = v;
        },
    };

    x.a &&= 42;
    x.a &&= 42;

    return log.join(" ") + " " + a;
}, "get get false");

shouldBe(function() {
    let log = [];

    let a = undefined;
    let x = {
        get a() {
            log.push("get");
            return a;
        },
        set a(v) {
            log.push("set");
            a = v;
        },
    };

    x.a &&= 42;
    x.a &&= 42;

    return log.join(" ") + " " + a;
}, "get get undefined");



shouldBe(function() {
    let log = [];

    let a = true;
    let x = {
        get a() {
            log.push("get");
            return a;
        },
        set a(v) {
            log.push("set");
            a = v;
        },
    };

    x["a"] &&= 42;
    x["a"] &&= 42;

    return log.join(" ") + " " + a;
}, "get set get set 42");

shouldBe(function() {
    let log = [];

    let a = false;
    let x = {
        get a() {
            log.push("get");
            return a;
        },
        set a(v) {
            log.push("set");
            a = v;
        },
    };

    x["a"] &&= 42;
    x["a"] &&= 42;

    return log.join(" ") + " " + a;
}, "get get false");

shouldBe(function() {
    let log = [];

    let a = undefined;
    let x = {
        get a() {
            log.push("get");
            return a;
        },
        set a(v) {
            log.push("set");
            a = v;
        },
    };

    x["a"] &&= 42;
    x["a"] &&= 42;

    return log.join(" ") + " " + a;
}, "get get undefined");



shouldBe(function() {
    let log = [];

    class Parent {
        get a() {
            log.push("get-parent");
            return this._a;
        }
        set a(v) {
            log.push("set-parent");
            this._a &&= v;
        }
    }
    class Child extends Parent {
        get a() {
            log.push("get-child");
            return super.a;
        }
        set a(v) {
            log.push("set-child");
            super.a &&= v;
        }
    }

    let x = new Child;
    x._a = true;
    x.a &&= 42;
    x.a &&= 42;

    return log.join(" ") + " " + x._a;
}, "get-child get-parent set-child get-parent set-parent get-child get-parent set-child get-parent set-parent 42");

shouldBe(function() {
    let log = [];

    class Parent {
        get a() {
            log.push("get-parent");
            return this._a;
        }
        set a(v) {
            log.push("set-parent");
            this._a &&= v;
        }
    }
    class Child extends Parent {
        get a() {
            log.push("get-child");
            return super.a;
        }
        set a(v) {
            log.push("set-child");
            super.a &&= v;
        }
    }

    let x = new Child;
    x._a = false;
    x.a &&= 42;
    x.a &&= 42;

    return log.join(" ") + " " + x._a;
}, "get-child get-parent get-child get-parent false");

shouldBe(function() {
    let log = [];

    class Parent {
        get a() {
            log.push("get-parent");
            return this._a;
        }
        set a(v) {
            log.push("set-parent");
            this._a &&= v;
        }
    }
    class Child extends Parent {
        get a() {
            log.push("get-child");
            return super.a;
        }
        set a(v) {
            log.push("set-child");
            super.a &&= v;
        }
    }

    let x = new Child;
    x._a = undefined;
    x.a &&= 42;
    x.a &&= 42;

    return log.join(" ") + " " + x._a;
}, "get-child get-parent get-child get-parent undefined");



shouldBe(function() {
    let log = [];

    class Parent {
        get a() {
            log.push("get-parent");
            return this._a;
        }
        set a(v) {
            log.push("set-parent");
            this._a &&= v;
        }
    }
    class Child extends Parent {
        get a() {
            log.push("get-child");
            return super["a"];
        }
        set a(v) {
            log.push("set-child");
            super["a"] &&= v;
        }
    }

    let x = new Child;
    x._a = true;
    x["a"] &&= 42;
    x["a"] &&= 42;

    return log.join(" ") + " " + x._a;
}, "get-child get-parent set-child get-parent set-parent get-child get-parent set-child get-parent set-parent 42");

shouldBe(function() {
    let log = [];

    class Parent {
        get a() {
            log.push("get-parent");
            return this._a;
        }
        set a(v) {
            log.push("set-parent");
            this._a &&= v;
        }
    }
    class Child extends Parent {
        get a() {
            log.push("get-child");
            return super["a"];
        }
        set a(v) {
            log.push("set-child");
            super["a"] &&= v;
        }
    }

    let x = new Child;
    x._a = false;
    x["a"] &&= 42;
    x["a"] &&= 42;

    return log.join(" ") + " " + x._a;
}, "get-child get-parent get-child get-parent false");

shouldBe(function() {
    let log = [];

    class Parent {
        get a() {
            log.push("get-parent");
            return this._a;
        }
        set a(v) {
            log.push("set-parent");
            this._a &&= v;
        }
    }
    class Child extends Parent {
        get a() {
            log.push("get-child");
            return super["a"];
        }
        set a(v) {
            log.push("set-child");
            super["a"] &&= v;
        }
    }

    let x = new Child;
    x._a = undefined;
    x["a"] &&= 42;
    x["a"] &&= 42;

    return log.join(" ") + " " + x._a;
}, "get-child get-parent get-child get-parent undefined");



shouldBe(function() {
    let x = {};
    Object.defineProperty(x, "a", {
        value: true,
        writable: false,
    });
    return x.a &&= 42;
}, 42);

shouldBe(function() {
    let x = {};
    Object.defineProperty(x, "a", {
        value: false,
        writable: false,
    });
    return x.a &&= 42;
}, false);

shouldBe(function() {
    let x = {};
    Object.defineProperty(x, "a", {
        value: undefined,
        writable: false,
    });
    return x.a &&= 42;
}, undefined);



shouldThrow(function() {
    "use strict";
    let x = {};
    Object.defineProperty(x, "a", {
        value: true,
        writable: false,
    });
    return x.a &&= 42;
}, TypeError);

shouldBe(function() {
    "use strict";
    let x = {};
    Object.defineProperty(x, "a", {
        value: false,
        writable: false,
    });
    return x.a &&= 42;
}, false);

shouldBe(function() {
    "use strict";
    let x = {};
    Object.defineProperty(x, "a", {
        value: undefined,
        writable: false,
    });
    return x.a &&= 42;
}, undefined);



shouldBe(function() {
    let x = {};
    Object.defineProperty(x, "a", {
        value: true,
        writable: false,
    });
    return x["a"] &&= 42;
}, 42);

shouldBe(function() {
    let x = {};
    Object.defineProperty(x, "a", {
        value: false,
        writable: false,
    });
    return x["a"] &&= 42;
}, false);

shouldBe(function() {
    let x = {};
    Object.defineProperty(x, "a", {
        value: undefined,
        writable: false,
    });
    return x["a"] &&= 42;
}, undefined);



shouldThrow(function() {
    "use strict";
    let x = {};
    Object.defineProperty(x, "a", {
        value: true,
        writable: false,
    });
    return x["a"] &&= 42;
}, TypeError);

shouldBe(function() {
    "use strict";
    let x = {};
    Object.defineProperty(x, "a", {
        value: false,
        writable: false,
    });
    return x["a"] &&= 42;
}, false);

shouldBe(function() {
    "use strict";
    let x = {};
    Object.defineProperty(x, "a", {
        value: undefined,
        writable: false,
    });
    return x["a"] &&= 42;
}, undefined);



shouldBe(function() {
    let x = true;
    (function() {
        x &&= 42;
    })();
    return x;
}, 42);

shouldBe(function() {
    let x = false;
    return (function() {
        return x &&= 42;
    })();
}, false);

shouldBe(function() {
    let x = undefined;
    return (function() {
        return x &&= 42;
    })();
}, undefined);



shouldThrow(function() {
    const x = true;
    (function() {
        x &&= 42;
    })();
    return x;
}, TypeError);

shouldBe(function() {
    const x = false;
    return (function() {
        return x &&= 42;
    })();
}, false);

shouldBe(function() {
    const x = undefined;
    return (function() {
        return x &&= 42;
    })();
}, undefined);



shouldBe(function() {
    let count = 0;

    const x = true;
    try {
        x &&= ++count;
    } catch { }

    return count;
}, 1);

shouldBe(function() {
    let count = 0;

    const x = false;
    x &&= ++count;

    return count;
}, 0);

shouldBe(function() {
    let count = 0;

    const x = undefined;
    x &&= ++count;

    return count;
}, 0);



shouldBe(function() {
    let count = 0;

    const x = true;
    function capture() { return x; }

    try {
        x &&= ++count;
    } catch { }

    return count;
}, 1);

shouldBe(function() {
    let count = 0;

    const x = false;
    function capture() { return x; }

    x &&= ++count;

    return count;
}, 0);

shouldBe(function() {
    let count = 0;

    const x = undefined;
    function capture() { return x; }

    x &&= ++count;

    return count;
}, 0);



shouldThrow(function() {
    x &&= 42;
    let x = true;
    return x;
}, ReferenceError);



shouldBe(function() {
    return undefined &&= 42;
}, undefined);

shouldThrowSyntaxError(`null &&= 42`);

shouldThrowSyntaxError(`true &&= 42`);

shouldThrowSyntaxError(`false &&= 42`);

shouldThrowSyntaxError(`0 &&= 42`);

shouldThrowSyntaxError(`1 &&= 42`);

shouldThrowSyntaxError(`"" &&= 42`);

shouldThrowSyntaxError(`"test" &&= 42`);

shouldThrowSyntaxError(`{} &&= 42`);

shouldThrowSyntaxError(`[] &&= 42`);



shouldBe(function() {
    let x = true;
    x &&= function() {};
    return x.name;
}, "x");

shouldBe(function() {
    let x = true;
    x &&= () => {};
    return x.name;
}, "x");

shouldBe(function() {
    let x = true;
    x &&= class {};
    return x.name;
}, "x");
