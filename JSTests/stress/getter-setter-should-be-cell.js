//@ runDefault("--validateAbstractInterpreterState=1", "--forceEagerCompilation=1")
String.__proto__ = createGlobalObject();
const that = {};
that.__proto__ = String;

function foo() {
    with (that) {
        function bar(a0, a1) {
            const v0 = '';
            const v1 = undefined;
            const v2 = undefined;
            const v3 = undefined;
            const p = { get: ()=>{} };
            for (let j = 0; j < 1; j++) {
                function f0() {}
                const v4 = Object.defineProperty(''.__proto__, '__proto__', p);
            }
            const v5 = undefined;
        }
        for (let i = 0; i < 100; i++) {
            new Promise(bar);
        }
    }
}

foo();
