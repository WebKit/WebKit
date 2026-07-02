description(
"This tests that expressions of the form x = x.apply(...) don't break bytecode generation."
);

function foo(a,b) {
    return a+b;
}

function bar() {
    var x;
    
    x = foo;
    
    var array = [];
    array.push(1);
    array.push(2);
    
    x = x.apply(void(0), array);
    
    return x;
}

shouldBe("bar()", "3");



