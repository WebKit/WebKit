function assert(b) {
    if (!b) {
        throw "bad assert!"
    }
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`
            bad error:      ${String(error)}
            expected error: ${errorMessage}
        `);
}

// ---------- single static block ---------- 
{
    var y = 'Outer y';

    class A {
        static field = 'Inner y';
        static {
            var y = this.field;
        }
    }

    assert(y === 'Outer y');
}

//  ---------- multiple static blocks ---------- 
{
    class C {
        static {
            assert(this.x === undefined);
        }
        static x = 10;
        static {
            assert(this.x === 10);
            assert(this.y === undefined);
        }
        static y = 20;
        static {
            assert(this.y === 20);
        }
    }
}

//  ---------- use this ---------- 
{
    class A {
        static field = 'A static field';
        static {
            assert(this.field === 'A static field');
        }
    }
}

//  ---------- use super ---------- 
{
    class A {
        static fieldA = 'A.fieldA';
    }
    class B extends A {
        static {
            assert(super.fieldA === 'A.fieldA');
        }
    }
}

//  ---------- access to private fields ---------- 
{
    let getDPrivateField;

    class D {
        #privateField;
        constructor(v) {
            this.#privateField = v;
        }
        static {
            getDPrivateField = (d) => d.#privateField;
        }
    }

    assert(getDPrivateField(new D('private')) === 'private');
}

// ---------- "friend" access ----------
{
    let A, B;

    let friendA;

    A = class A {
        #x;
        constructor(x) {
            this.#x = x;
        }
        static {
            friendA = {
                getX(obj) { return obj.#x },
                setX(obj, value) { obj.#x = value }
            };
        }
        getX() {
            return this.#x;
        }
    };

    B = class B {
        constructor(a) {
            const x = friendA.getX(a);
            friendA.setX(a, x + 32);
        }
    };

    let a = new A(10);
    new B(a);

    assert(a.getX() === 42);
}

// ---------- break ----------
{
    class C {
        static {
            while (false)
                break;           // isStaticBlock = true, isValid = false, isCurrentScopeValid = true
        }
    }
}

{
    class C {
        static {
            while (false) {
                break;           // isStaticBlock = true, isValid = true, isCurrentScopeValid = false
            }
        }
    }
}

shouldThrow(() => {
    eval(`
        while (false) {
            class C {
                static {
                    break;       // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
                }
            }
        }
    `);
}, `SyntaxError: 'break' cannot cross static block boundary.`);

shouldThrow(() => {
    eval(`
        class C {
            static {
                break;           // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
            }
        }
    `);
}, `SyntaxError: 'break' cannot cross static block boundary.`);

shouldThrow(() => {
    eval(`
        while (false) {
            class C {
                static {
                    {
                        break;   // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
                    }
                }
            }
        }
    `);
}, `SyntaxError: 'break' cannot cross static block boundary.`);

shouldThrow(() => {
    eval(`
        class C {
            static {
                {
                    break;        // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
                }
            }
        }
    `);
}, `SyntaxError: 'break' cannot cross static block boundary.`);

// ---------- continue ----------
{
    class C {
        static {
            while (false)
                continue;           // isStaticBlock = true, isValid = false, isCurrentScopeValid = true
        }
    }
}

{
    class C {
        static {
            while (false) {
                continue;           // isStaticBlock = true, isValid = true, isCurrentScopeValid = false
            }
        }
    }
}

shouldThrow(() => {
    eval(`
        while (false) {
            class C {
                static {
                    continue;       // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
                }
            }
        }
    `);
}, `SyntaxError: 'continue' cannot cross static block boundary.`);

shouldThrow(() => {
    eval(`
        class C {
            static {
                continue;           // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
            }
        }
    `);
}, `SyntaxError: 'continue' cannot cross static block boundary.`);

shouldThrow(() => {
    eval(`
        while (false) {
            class C {
                static {
                    {
                        continue;   // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
                    }
                }
            }
        }
    `);
}, `SyntaxError: 'continue' cannot cross static block boundary.`);

shouldThrow(() => {
    eval(`
        class C {
            static {
                {
                    continue;       // isStaticBlock = true, isValid = false, isCurrentScopeValid = false
                }
            }
        }
    `);
}, `SyntaxError: 'continue' cannot cross static block boundary.`);

// ---------- arguments ----------
{
    class C {
        static {
            function inner() {
                [arguments]();
            }
        }
    }
}

{
    class C {
        static {
            class B {
                inner() {
                    [arguments]();
                }
            }
        }
    }
}

{
    class C {
        static {
            function inner() {
                arguments[0];
            }
        }
    }
}

{
    class C {
        static {
            class B {
                inner() {
                    arguments[0];
                }
            }
        }
    }
}

{
    class C {
        static {
            (a, b) => {
                this.arguments[0];
            };
        }
    }
}

shouldThrow(() => {
    eval(`
        class C {
            static {
                arguments;
            }
        }
    `);
}, `SyntaxError: Cannot use 'arguments' as an identifier in static block.`);

// ---------- yield ----------

{
    class A {
        static {
            function* gen() {
                yield 42;
            }
        }
    }
}

{
    class A {
        static {
            class B {
                *gen() {
                    yield 42;
                }
            }
        }

    }
}

shouldThrow(() => {
    eval(`
        class C {
            static {
                function inner() {
                    yield 0;
                }
            }
        }
    `);
}, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.`);

shouldThrow(() => {
    eval(`
        class C {
            static {
                yield 0;
            }
        }
    `);
}, `SyntaxError: Unexpected keyword 'yield'. Cannot use 'yield' within static block.`);

// ---------- await ----------
{
    class C {
        static {
            function inner() {
                try { } catch (await) { }
            }
        }
    }
}

{
    class C {
        static {
            class B {
                inner() {
                    try { } catch (await) { }
                }
            }
        }
    }
}

{
    class C {
        static {
            async function inner() {
                await 0;
            }
        }
    }
}

{
    class C {
        static {
            class B {
                async inner() {
                    await 0;
                }
            }
        }
    }
}

{
    class C {
        static {
            function inner() {
                let await = 10;
            }
        }
    }
}

{
    class C {
        static {
            class B {
                inner() {
                    let await = 10;
                }
            }
        }
    }
}

{
    async function outer() {
        class C {
            static {
                async function inner() {
                    await 0;
                }
            }
        }
    }
}

{
    async function outer() {
        class C {
            static {
                class B {
                    async inner() {
                        await 0;
                    }
                }
            }
        }
    }
}

{
    async function outer() {
        class C {
            static {
                function inner() {
                    let await = 10;
                }
            }
        }
    }
}

{
    async function outer() {
        class C {
            static {
                class B {
                    inner() {
                        let await = 10;
                    }
                }
            }
        }
    }
}

{
    class C {
        static {
            function inner() {
                [await]();
            }
        }
    }
}

{
    class C {
        static {
            class B {
                inner() {
                    [await]();
                }
            }
        }
    }
}

// ---------- others ----------
{
    function doSomethingWith(x) {
        return {
            y: x + 1,
            z: x + 2
        };
    }

    class C {
        static x = 10;
        static y;
        static z;
        static {
            try {
                const obj = doSomethingWith(this.x);
                C.y = obj.y;
                C["z"] = obj.z;
            } catch {
            }
        }
    }

    assert(C.y === 11);
    assert(C.z === 12);
}

{
    class C {
        static y;
        static z;
        static {
            try {
                throw "err";
            } catch {
                C.y = 13;
                C['z'] = 14;
            }
        }
    }

    assert(C.y === 13);
    assert(C.z === 14);
}

{
    var value = null;
    class C {
        static {
            function inner() {
                value = new.target;
            }
            inner();
        }
    }
    assert(value === undefined);
}

{
    class C {
        static {
            value = new.target;
        }
    }
    assert(value === undefined);
}

{
    class C {
        static {
            function inner() {
                {
                    return 10;
                }
            }
        }
    }
}

{
    class C {
        static {
            function inner() {
                Promise.resolve().then(makeMasquerader(), makeMasquerader());
            }
            {
                Promise.resolve().then(makeMasquerader(), makeMasquerader());
            }
        }
    }
}

{
    class C {
        static {
            {
                function foo(arg) {
                    let o;
                    if (arg) {
                        o = {};
                    } else {
                        o = function() { }
                    }
                    return typeof o;
                }
                noInline(foo);
                
                for (let i = 0; i < 10000; i++) {
                    let bool = !!(i % 2);
                    let result = foo(bool);
                    if (bool)
                        assert(result === "object");
                    else
                        assert(result === "function");
                }
            }
        }
    }
}

{
    function foo() {
        assert(foo.caller === null);
    }
    class C {
        static {
            foo();
        }
    }
}
