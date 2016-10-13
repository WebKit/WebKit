let a=function(){return 0}
let b=function(){return 0}
let c=function(){return 0}
let arr=[];
let obj={};

// Control flow

if (0)
    a();

if (0) {
    a();
}

if (0)
    a();
else if (0)
    a();
else
    a();

a() ? b() : c()

// Loops

while (0)
    a();

while (0) {
    a();
}

do {
    a();
} while (0);

for (a(); b(); c())
    break;

for (; b(); c())
    break;

for (a(); b();)
    break;

for (a();; c())
    break;

for (a();;)
    break;

for (; b();)
    break;

for (;; c())
    break;

for (;;)
    break;

for (a(); b(); c()) {
    break;
}

for (let x of arr)
    break;

for (let x in obj)
    break;

// Switch

switch (0) {
case 1:
    a();
    break;
case 2:
    a();
    // fallthrough
case 3:
    a();
    break;
default:
    a();
    break;
}

// Try/Catch

try {
    a();
} catch (e) {
    shouldNotBeReached();
} finally {
    b();
}

// Class

class Base {
    constructor()
    {
        this._base = true;
    }

    baseMethod()
    {
        a();
    }

    method()
    {
        a();
    }
}

class Child extends Base {
    constructor()
    {
        super();
        this._child = true;
    }

    childMethod()
    {
        b();
    }

    method()
    {
        super.method();
        b();
    }

    get name()
    {
        return this._name;
    }
    
    set name(x)
    {
        this._name = x;
    }
}

// ---------
/* Misc */
// ---------

    {
        a();
    }

label:
    {
        a();
    }

var w1 = {x:1, y:2};
with (w1) {
    a();
}

var v1 = 1,
    v2 = 1;
let l1 = 2,
    l2 = 2;
const c1 = 3,
    c2 = 3;

v1 = v2 = v1;

var {x, y} = obj;
var [w, z] = arr;

var o1 = {
    p1: 1,
    p2: a(),
    p3: 1,
    ["p4"]: 1,
    [b()]: 1,
};

var a1 = [
    1,
    a(),
    1,
    b(),
];

var i1 = new Base;
var i2 = new Child;
i2.name;
i2.name = 1;
i2.method();

var t1 = `${1} ${x=1} ${a()}`;
var t2 = a`${1} ${x=1} ${a()}`;

a(a(), b());

if (o1.p1)
    a();

if (o1["p1"])
    a();

if (String.raw`test`)
    a();
