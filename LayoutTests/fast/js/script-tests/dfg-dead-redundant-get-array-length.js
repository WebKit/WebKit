description(
"Tests that having a dead, and redundant, use of array.length doesn't cause compiler crashes."
);

function foo(array) {
    var x = array.length; // This is dead. If it wasn't, it would be redundant with the one below.
    return array.length;
}

dfgShouldBe(foo, "foo([1, 2, 3])", "3");
