function* gen() {}
function* gen() {
    1
}
function* gen() {}
function* gen() {
    1
}
function* gen() {}
function* gen() {
    1
}

function* gen() {
    yield
}
function* gen() {
    yield x
}
function* gen() {
    yield "x"
}
function* gen() {
    yield [x]
}
function* gen() {
    yield foo()
}

function* gen(a=1, [b, c], ...rest) {
    return yield yield foo("foo")
}
