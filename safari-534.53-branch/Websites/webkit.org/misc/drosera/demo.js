function nestedFunctions(str) {
    var str2 = str + "fg";
    var result = rot13(str2);
    var element = document.getElementById("rotResult");
    element.innerText = "Result: " + result;
}

function rot(t, u, v) {
     return String.fromCharCode(((t - u + v) % (v * 2)) + u);
}

function rot13(s) {
    console.profile("ROT 13");

    var b = [], c, i = s.length,
    a = 'a'.charCodeAt(), z = a + 26,
    A = 'A'.charCodeAt(), Z = A + 26;

    while(i--) {
        c = s.charCodeAt(i);
        if(c >= a && c < z )
            b[i] = rot(c, a, 13);
        else if(c >= A && c < Z)
            b[i] = rot(c, A, 13);
        else
            b[i] = s.charAt(i);
    }

    var result = b.join('');
    console.profileEnd("ROT 13");
    return result;
}

function append(a, b) {
    return a + " " + b;
}

function withTest(event) {
    var object = { x: 100, y: 200 };
    with( object ) {
        var result = append(x, y);
        return result;
    }
}

function functionFactory(n) {
    var x = n;    
    return function() {
        return n * x;
    };
}

function functionFactoryTest() {
    var func = functionFactory(10);
    var result = func();
}

function loopsTest() {
    console.profile("Loop Test");
    var x = 0;
    for(var i = 0; i < 5; i++)
        x += i;
    console.profileEnd("Loop Test");
}

function throwFoo() {
    // throw new Error("Foo");
    document.dssssssdf.test = 1;
}

function throwBar() {
    throwFoo();
}

function throwException() {
    throwBar();
}

function foo() {
    i = 1;
}

function bar() {
    i = 2;
    foo();
}

function baz() {
    i = 3;
    bar();
}

function quux() {
    i = 4;
    baz();
}

function throwExceptionSoon() {
    setTimeout( function() { document.dssssssdf.test = 1; }, 1000 );
}
