// Large JavaScript file for testing

function a() {
    // Function a
    let x = 10;
    for (let i = 0; i < 100; i++) {
        x += i;
    }
    return x;
}

function b() {
    // Function b
    let y = 20;
    let z = a();
    return y + z;
}

function c() {
    // Function c
    let arr = [];
    for (let i = 0; i < 1000; i++) {
        arr.push({
            id: i,
            value: Math.random()
        });
    }
    return arr;
}

function d() {
    // Function d
    let obj = {};
    for (let i = 0; i < 500; i++) {
        obj['key' + i] = 'value' + i;
    }
    return obj;
}

function e() {
    // Function e
    let text = "This is a long string of text. ".repeat(100);
    return text;
}

function f() {
    // Function f
    let result = 0;
    for (let i = 0; i < 1000; i++) {
        result += Math.sqrt(i);
    }
    return result;
}

function g() {
    // Function g
    let date = new Date();
    return date.toString();
}

function h() {
    // Function h
    let regex = new RegExp('^[a-zA-Z0-9]*$');
    return regex.test('someTestString123');
}

function i() {
    // Function i
    let promise = new Promise((resolve, reject) => {
        setTimeout(() => {
            resolve("Promise resolved after 1 second");
        }, 1000);
    });
    return promise;
}

async function j() {
    // Function j
    let result = await i();
    console.log(result);
}

class MyClass {
    constructor() {
        this.property1 = 'value1';
        this.property2 = 123;
    }

    method1() {
        return this.property1;
    }

    method2() {
        return this.property2;
    }
}

const instance = new MyClass();

console.log(a());
console.log(b());
console.log(c());
console.log(d());
console.log(e());
console.log(f());
console.log(g());
console.log(h());
j();
console.log(instance.method1());
console.log(instance.method2());
