function foo() {
    foo();
}

function start() {
    try {
        foo();
    } catch(e) {
        10 + 10; 
    }
}
