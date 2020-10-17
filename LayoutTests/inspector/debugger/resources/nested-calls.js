function inner() {
    let x = 42;
    return x;
}

function outer() {
    inner();
}

outer();
