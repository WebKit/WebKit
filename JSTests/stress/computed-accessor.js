function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

// Class.
(function () {
    {
        class A {
            get ['a' + 'b']() {
                return 42;
            }
        }
        let a = new A();
        shouldBe(a.ab, 42);
        a.ab = 20000;
        shouldBe(a.ab, 42);
    }

    {
        class A {
            get ['a' + '0']() {
                return 42;
            }
        }
        let a = new A();
        shouldBe(a.a0, 42);
        a.a0 = 20000;
        shouldBe(a.a0, 42);
    }

    {
        class A {
            get ['1' + '0']() {
                return 42;
            }
        }
        let a = new A();
        shouldBe(a[10], 42);
        a[10] = 20000;
        shouldBe(a[10], 42);
    }

    {
        class A {
            get [0.1]() {
                return 42;
            }
        }
        let a = new A();
        shouldBe(a[0.1], 42);
        a[0.1] = 20000;
        shouldBe(a[0.1], 42);
    }

    {
        class A {
            get [10.50]() {
                return 42;
            }
        }
        let a = new A();
        shouldBe(a[10.5], 42);
        a[10.5] = 20000;
        shouldBe(a[10.5], 42);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            get [hello()]() {
                return 42;
            }
        }
        let a = new A();
        shouldBe(a.ok, 42);
        a.ok = 20000;
        shouldBe(a.ok, 42);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            get [hello()]() {
                return 42;
            }
        }
        class Derived extends A { }

        let a = new Derived();
        shouldBe(a.ok, 42);
        a.ok = 20000;
        shouldBe(a.ok, 42);
    }

    {
        class A {
            set ['a' + 'b'](value) {
                this.value = value;
            }
        }
        let a = new A();
        a.ab = 42;
        shouldBe(a.value, 42);
        shouldBe(a.ab, undefined);
    }

    {
        class A {
            set ['a' + '0'](value) {
                this.value = value;
            }
        }
        let a = new A();
        a.a0 = 42;
        shouldBe(a.value, 42);
        shouldBe(a.a0, undefined);
    }

    {
        class A {
            set ['1' + '0'](value) {
                this.value = value;
            }
        }
        let a = new A();
        a[10] = 42;
        shouldBe(a.value, 42);
        shouldBe(a[10], undefined);
    }

    {
        class A {
            set [0.1](value) {
                this.value = value;
            }
        }
        let a = new A();
        a[0.1] = 42;
        shouldBe(a.value, 42);
        shouldBe(a[0.1], undefined);
    }

    {
        class A {
            set [10.50](value) {
                this.value = value;
            }
        }
        let a = new A();
        a[10.5] = 42;
        shouldBe(a.value, 42);
        shouldBe(a[10.5], undefined);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            set [hello()](value) {
                this.value = value;
            }
        }
        let a = new A();
        a.ok = 42;
        shouldBe(a.value, 42);
        shouldBe(a.ok, undefined);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            set [hello()](value) {
                this.value = value;
            }
        }
        class Derived extends A { }

        let a = new Derived();
        a.ok = 42;
        shouldBe(a.value, 42);
        shouldBe(a.ok, undefined);
    }
}());

// Class static.
(function () {
    {
        class A {
            static get ['a' + 'b']() {
                return 42;
            }
        }
        shouldBe(A.ab, 42);
        A.ab = 20000;
        shouldBe(A.ab, 42);
    }

    {
        class A {
            static get ['a' + '0']() {
                return 42;
            }
        }
        shouldBe(A.a0, 42);
        A.a0 = 20000;
        shouldBe(A.a0, 42);
    }

    {
        class A {
            static get ['1' + '0']() {
                return 42;
            }
        }
        shouldBe(A[10], 42);
        A[10] = 20000;
        shouldBe(A[10], 42);
    }

    {
        class A {
            static get [0.1]() {
                return 42;
            }
        }
        shouldBe(A[0.1], 42);
        A[0.1] = 20000;
        shouldBe(A[0.1], 42);
    }

    {
        class A {
            static get [10.50]() {
                return 42;
            }
        }
        shouldBe(A[10.5], 42);
        A[10.5] = 20000;
        shouldBe(A[10.5], 42);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            static get [hello()]() {
                return 42;
            }
        }
        shouldBe(A.ok, 42);
        A.ok = 20000;
        shouldBe(A.ok, 42);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            static get [hello()]() {
                return 42;
            }
        }
        class Derived extends A { }

        shouldBe(Derived.ok, 42);
        Derived.ok = 20000;
        shouldBe(Derived.ok, 42);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            static get [hello()]() {
                return 42;
            }
        }
        class Derived extends A { }

        shouldBe(Derived.ok, 42);
        Derived.ok = 20000;
        shouldBe(Derived.ok, 42);
    }

    {
        class A {
            static set ['a' + 'b'](value) {
                this.value = value;
            }
        }
        A.ab = 42;
        shouldBe(A.value, 42);
        shouldBe(A.ab, undefined);
    }

    {
        class A {
            static set ['a' + '0'](value) {
                this.value = value;
            }
        }
        A.a0 = 42;
        shouldBe(A.value, 42);
        shouldBe(A.a0, undefined);
    }

    {
        class A {
            static set ['1' + '0'](value) {
                this.value = value;
            }
        }
        A[10] = 42;
        shouldBe(A.value, 42);
        shouldBe(A[10], undefined);
    }

    {
        class A {
            static set [0.1](value) {
                this.value = value;
            }
        }
        A[0.1] = 42;
        shouldBe(A.value, 42);
        shouldBe(A[0.1], undefined);
    }

    {
        class A {
            static set [10.50](value) {
                this.value = value;
            }
        }
        A[10.5] = 42;
        shouldBe(A.value, 42);
        shouldBe(A[10.5], undefined);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            static set [hello()](value) {
                this.value = value;
            }
        }
        A.ok = 42;
        shouldBe(A.value, 42);
        shouldBe(A.ok, undefined);
    }

    {
        function hello() {
            return 'ok';
        }
        class A {
            static set [hello()](value) {
                this.value = value;
            }
        }
        class Derived extends A { }

        Derived.ok = 42;
        shouldBe(Derived.value, 42);
        shouldBe(Derived.ok, undefined);
    }
}());


// Object.
(function () {
    {
        var a = {
            get ['a' + 'b']() {
                return 42;
            }
        }
        shouldBe(a.ab, 42);
        a.ab = 20000;
        shouldBe(a.ab, 42);
    }

    {
        var a = {
            get ['a' + '0']() {
                return 42;
            }
        }
        shouldBe(a.a0, 42);
        a.a0 = 20000;
        shouldBe(a.a0, 42);
    }

    {
        var a = {
            get ['1' + '0']() {
                return 42;
            }
        }
        shouldBe(a[10], 42);
        a[10] = 20000;
        shouldBe(a[10], 42);
    }

    {
        var a = {
            get [0.1]() {
                return 42;
            }
        }
        shouldBe(a[0.1], 42);
        a[0.1] = 20000;
        shouldBe(a[0.1], 42);
    }

    {
        var a = {
            get [10.50]() {
                return 42;
            }
        }
        shouldBe(a[10.5], 42);
        a[10.5] = 20000;
        shouldBe(a[10.5], 42);
    }

    {
        function hello() {
            return 'ok';
        }
        var a = {
            get [hello()]() {
                return 42;
            }
        }
        shouldBe(a.ok, 42);
        a.ok = 20000;
        shouldBe(a.ok, 42);
    }

    {
        var a = {
            set ['a' + 'b'](value) {
                this.value = value;
            }
        }
        a.ab = 42;
        shouldBe(a.value, 42);
        shouldBe(a.ab, undefined);
    }

    {
        var a = {
            set ['a' + '0'](value) {
                this.value = value;
            }
        }
        a.a0 = 42;
        shouldBe(a.value, 42);
        shouldBe(a.a0, undefined);
    }

    {
        var a = {
            set ['1' + '0'](value) {
                this.value = value;
            }
        }
        a[10] = 42;
        shouldBe(a.value, 42);
        shouldBe(a[10], undefined);
    }

    {
        var a = {
            set [0.1](value) {
                this.value = value;
            }
        }
        a[0.1] = 42;
        shouldBe(a.value, 42);
        shouldBe(a[0.1], undefined);
    }

    {
        var a = {
            set [10.50](value) {
                this.value = value;
            }
        }
        a[10.5] = 42;
        shouldBe(a.value, 42);
        shouldBe(a[10.5], undefined);
    }

    {
        function hello() {
            return 'ok';
        }
        var a = {
            set [hello()](value) {
                this.value = value;
            }
        }
        a.ok = 42;
        shouldBe(a.value, 42);
        shouldBe(a.ok, undefined);
    }
}());
