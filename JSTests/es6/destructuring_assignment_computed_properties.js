function test() {
    var steps = [];
    var backingStore = {};

    function storeProperty(name, value) {
        steps.push(`store: ${name} = ${value}`);
        backingStore[name] = value;
    }

    function computeName(name) {
        steps.push(`compute: ${name}`);
        return name;
    }

    function loadValue(name, value) {
        steps.push(`load: ${name} > ${value}`);
        return value;
    }

    var out = {
        get a() { return backingStore.a; },
        set a(v) { storeProperty("a", v); },
        get b() { return backingStore.b; },
        set b(v) { storeProperty("b", v); },
        get c() { return backingStore.c; },
        set c(v) { storeProperty("c", v); },
        get d() { return backingStore.d; },
        set d(v) { storeProperty("d", v); }
    };
    ({
        [computeName("propA")]: out.a,
        [computeName("propB")]: out.b,
        [computeName("propC")]: [...out["c"]],
        [computeName("propD")]: out.d = "default"
    } = {
        get propA() { return loadValue("propA", "hello"); },
        get propB() { return loadValue("propB", "world"); },
        get propC() { return loadValue("propC", [1, 2, 3]); },
        get propD() { return loadValue("propD"); }
    });

    var expectedSteps = [
        "compute: propA",
        "load: propA > hello",
        "store: a = hello",

        "compute: propB",
        "load: propB > world",
        "store: b = world",

        "compute: propC",
        "load: propC > 1,2,3",
        "store: c = 1,2,3",

        "compute: propD",
        "load: propD > undefined",
        "store: d = default"
    ];

    if (expectedSteps.length !== steps.length)
        return false;
    for (var i = 0; i < expectedSteps.length; ++i)
        if (expectedSteps[i] !== steps[i])
            return false;
    if (`${backingStore.a} ${backingStore.b} ${backingStore.c.join(":")}` !== "hello world 1:2:3")
        return false;

    return true;
}

if (!test())
    throw new Error("Test failed");
