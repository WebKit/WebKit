description(
"Tests that for-of iteration does not crashes."
);

function foo() {
    var o = Object.create(Array.prototype);
    for (var x of o) {
    }
}
foo();
