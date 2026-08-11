//@ runDefault("--useDollarVM=1", "--useConcurrentJIT=false", "--jitPolicyScale=0.001")

function main() {
    function createPoly() {
        function f() {}

        const object = new f();
        object.__proto__ = {};

        return new f();
    }

    $vm.createCustomTestGetterSetter();

    for (let i = 0; i < 50; i++) {
        createPoly();
    }

    let obj = createPoly();
    obj.__proto__ = $vm.createCustomTestGetterSetter();

    function opt(x) {
        return [x.customAccessor, Math.random(), Math.random(), Math.random(), Math.random(), Math.random()];
    }

    for (let i = 0; i < 1000; i++) {
        opt(obj);
    }

    obj = null;
    gc();

    $vm.createCustomTestGetterSetter();

    for (let i = 0; i < 10; i++) {
        opt({});
    }
}

main();

