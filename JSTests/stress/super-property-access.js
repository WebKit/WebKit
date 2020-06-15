Object.defineProperty(Object.prototype, "__proto__", {get: undefined});

function assert(b, m = "Bad!") {
    if (!b) {
        throw new Error(m);
    }
}

function test(f, iters = 1000) {
    for (let i = 0; i < iters; i++)
        f();
}

function func(x) {
    return x;
}
noInline(func);

test(function() {
    class A {
        constructor(x)
        {
            this._value = x;
        }

        set value(x) { this._value = x; }
        get value() { return this._value; }
    }

    class B extends A {
        set value(x) { super.value = x; }
        get value() { return super.value; }
    }

    let arr = [];
    for (let i = 0; i < 1000; i++) {
        arr.push(new B(20));
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === 20);
    }
    for (let i = 0; i < 1000; i++) {
        arr[i].value = i;
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }
}, 2);

test(function() {
    function value() { return 'value'; }
    noInline(value);

    class A {
        constructor(x, f = func)
        {
            this._value = x;
            this._func = f;
        }

        set value(x) { this._value = x; }
        get value() { return this._value; }
        get func() { return this._func; }
    }

    class B extends A {
        set value(x) { super[value()] = x; }
        get value() { return super[value()]; }
        inc() { return super[value()]++; }
        dec() { return super[value()]--; }
        preInc() { return ++super[value()]; }
        preDec() { return --super[value()]; }
        plusEq(x) { super[value()] += x; }
        minusEq(x) { super[value()] -= x; }
        timesEq(x) { super[value()] *= x; }
        divEq(x) { super[value()] /= x; }

        funcDot(x) { return super.func(x); }
        funcBracket(x) { return super.func(x); }
    }

    let arr = [];
    for (let i = 0; i < 1000; i++) {
        arr.push(new B(20));
    }
    for (let i = 0; i < 1000; i++) {
        let t = arr[i].value;
        assert(t === 20);
    }
    for (let i = 0; i < 1000; i++) {
        arr[i].value = i;
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].inc();
        assert(v === i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i+1);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].dec();
        assert(v === i+1);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].preInc();
        assert(v === i+1);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i+1);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].preDec();
        assert(v === i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i].plusEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i+i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i].minusEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i].timesEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i*i);
    }

    for (let i = 0; i < 1000; i++) {
        if (i === 0)
            arr[i].value = 0;
        else
            arr[i].divEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i] = new B(0, function(a) { return i + a; });
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].funcDot(i) === i + i);
        assert(arr[i].funcBracket(i*2) === i + i*2);
    }

}, 2);


test(function() {
    class A {
        constructor(x, f = func)
        {
            this._value = x;
            this._func = f;
        }

        set value(x) { this._value = x; }
        get value() { return this._value; }
        get func() { return this._func; }
    }

    class B extends A {
        set value(x) { (() => super.value = x)(); }
        get value() { return (() => super.value)(); }
        inc() { return (() => super.value++)(); }
        dec() { return (() => super.value--)(); }
        preInc() { return (() => ++super.value)(); }
        preDec() { return (() => --super.value)(); }
        plusEq(x) { (() => super.value += x)(); }
        minusEq(x) { (() => super.value -= x)(); }
        timesEq(x) { (() => super.value *= x)(); }
        divEq(x) { (() => super.value /= x)(); }

        funcDot(x) { return (() => super.func(x))(); }
        funcBracket(x) { return (() => super.func(x))(); }
    }

    let arr = [];
    for (let i = 0; i < 1000; i++) {
        arr.push(new B(20));
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === 20);
    }
    for (let i = 0; i < 1000; i++) {
        arr[i].value = i;
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].inc();
        assert(v === i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i+1);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].dec();
        assert(v === i+1);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].preInc();
        assert(v === i+1);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i+1);
    }

    for (let i = 0; i < 1000; i++) {
        let v = arr[i].preDec();
        assert(v === i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i].plusEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i+i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i].minusEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i].timesEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i*i);
    }

    for (let i = 0; i < 1000; i++) {
        if (i === 0)
            arr[i].value = 0;
        else
            arr[i].divEq(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].value === i);
    }

    for (let i = 0; i < 1000; i++) {
        arr[i] = new B(0, function(a) { return i + a; });
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].funcDot(i) === i + i);
        assert(arr[i].funcBracket(i*2) === i + i*2);
    }

}, 2);

test(function() {
    function foo() { return 'foo'; }
    noInline(foo);
    class A { }
    let obj = {};
    A.prototype.foo = obj;
    A.prototype[0] = obj;


    class B extends A {
        baz() { return super[foo()]; }
        jaz() { return super.foo; }
        bar() { return super[0]; }
        
    }

    assert((new B).baz() === obj);
    assert((new B).jaz() === obj);
    assert((new B).bar() === obj);
});

test(function() {
    class A { }
    for (let i = 0; i < 1000; i++)
        A.prototype[i] = i;


    class B extends A {
        index(i) { return super[i]; }
    }

    let b = new B;
    for (let i = 0; i < 1000; i++) {
        assert(b.index(i) === i);
    }
}, 2);

test(function() {
    let obj = {};
    class A { constructor(r) { this._foo = r; } }
    Object.defineProperty(A.prototype, '0', { get: function() { return this._foo; } });

    class B extends A {
        bar() { return super[0]; }
    }

    let rand = Math.random();
    assert((new B(rand)).bar() === rand);
});

test(function() { class A {
        constructor() { this._array = []; }
        set foo(x) {
            this._array.push(x);
        }
        get array() { return this._array; }
    }

    class B extends A {
        baz(i) {
            let o = {x:20, y:30, [i]:i};
            for (super.foo in o) { }
        }
    }
    let arr = [];
    for (let i = 0; i < 20; i++)
        arr.push(new B);
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i];
        obj.baz(i);
    }
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i].array;
        assert(obj.length === 3)
        assert(obj[0] === '' + i);
        assert(obj[1] === 'x')
        assert(obj[2] === 'y')
    }
}, 100);

test(function() {
    function foo() { return 'foo'; }
    noInline(foo);
    class A {
        constructor() { this._array = []; }
        set foo(x) {
            this._array.push(x);
        }
        get array() { return this._array; }
    }

    class B extends A {
        baz(i) {
            let o = {x:20, y:30, [i]:i};
            for (super[foo()] in o) { }
        }
    }
    let arr = [];
    for (let i = 0; i < 20; i++)
        arr.push(new B);
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i];
        obj.baz(i);
    }
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i].array;
        assert(obj.length === 3)
        assert(obj[0] === '' + i);
        assert(obj[1] === 'x')
        assert(obj[2] === 'y')
    }
}, 100);

test(function() {
    class A {
        constructor() { this._array = []; }
        set foo(x) {
            this._array.push(x);
        }
        get array() { return this._array; }
    }

    class B extends A {
        baz(i) {
            let o = ['' + i, "x", "y"];
            for (super.foo of o) { }
        }
    }
    let arr = [];
    for (let i = 0; i < 20; i++)
        arr.push(new B);
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i];
        obj.baz(i);
    }
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i].array;
        assert(obj.length === 3)
        assert(obj[0] === '' + i);
        assert(obj[1] === 'x')
        assert(obj[2] === 'y')
    }
}, 100);

test(function() {
    function foo() { return 'foo'; }
    class A {
        constructor() { this._array = []; }
        set foo(x) {
            this._array.push(x);
        }
        get array() { return this._array; }
    }

    class B extends A {
        baz(i) {
            let o = ['' + i, "x", "y"];
            for (super[foo()] of o) { }
        }
    }
    let arr = [];
    for (let i = 0; i < 20; i++)
        arr.push(new B);
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i];
        obj.baz(i);
    }
    for (let i = 0; i < arr.length; i++) {
        let obj = arr[i].array;
        assert(obj.length === 3)
        assert(obj[0] === '' + i);
        assert(obj[1] === 'x')
        assert(obj[2] === 'y')
    }
}, 100);

test(function() {
    class A {
        constructor() {
            this._foo = null;
        }
        set foo(x) {
            this._foo = x;
        }
        get foo() { return this._foo; }
    }
    function obj(i) { return {o: i}; }
    noInline(obj);

    class B extends A {
        baz(i) {
            ;({o: super.foo} = obj(i));
        }
    }
    let arr = [];
    for (let i = 0; i < 1000; i++) {
        arr.push((new B));
    }
    for (let i = 0; i < 1000; i++) {
        arr[i].baz(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].foo === i);
    }
}, 10);

test(function() {
    function foo() { return 'foo'; }
    noInline(foo);
    class A {
        constructor() {
            this._foo = null;
        }
        set foo(x) {
            this._foo = x;
        }
        get foo() { return this._foo; }
    }
    function obj(i) { return {o: i}; }
    noInline(obj);

    class B extends A {
        baz(i) {
            ;({o: super[foo()]} = obj(i));
        }
    }
    let arr = [];
    for (let i = 0; i < 1000; i++) {
        arr.push((new B));
    }
    for (let i = 0; i < 1000; i++) {
        arr[i].baz(i);
    }
    for (let i = 0; i < 1000; i++) {
        assert(arr[i].foo === i);
    }
}, 10);

test(function() {
    class A {
        constructor() {
            this._foo = null;
        }
        get call() {
            let ret = () => 'call';
            noInline(ret);
            return ret;
        }
        get apply() { 
            let ret = () => 'apply';
            noInline(ret);
            return ret;
        }
    }

    class B extends A {
        foo() {
            return super.call({}, 20, 30);
        }
        bar() {
            return super.apply({}, [10, 20]);
        }
    }
    for (let i = 0; i < 1000; i++) {
        let b = new B;
        assert(b.foo() === 'call');
        assert(b.bar() === 'apply');
    }
}, 2);

test(function() {
    class A {
        constructor(i) { this._i = i; }
        get foo() {
            return function(strings, ...values) {
                return [strings, values];
            }
        }
        get i() { return this._i; }
    }

    class B extends A {
        baz() {
            return super.foo`hello${super.i}world`;
        }
    }

    let arr = [];
    for (let i = 0; i < 1000; i++) {
        let b = new B(i);
        arr.push(b);
    }
    for (let i = 0; i < 1000; i++) {
        let r = arr[i].baz();
        assert(r.length === 2);
        assert(r[0].length === 2);
        assert(r[0][0] === 'hello');
        assert(r[0][1] === 'world');
        assert(r[1].length === 1);
        assert(r[1][0] === i);
    }
}, 10);
