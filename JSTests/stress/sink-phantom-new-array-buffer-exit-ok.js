function foo() {
}
function bar() {
    foo(...[42]);
}
for (var i = 0; i < 400000; i++) {
    bar();
}
