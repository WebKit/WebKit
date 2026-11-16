(function() {
    class A {}
    class B extends A {}
    class C extends B {}

    for (var j = 0; j < 1e5; j++)
        C["foo" + j] = j;
})();
