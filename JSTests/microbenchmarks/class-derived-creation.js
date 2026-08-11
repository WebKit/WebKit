class A {};
for (var i = 0; i < 1e4; ++i) {
    class B extends A {};
    class C extends B {};
    class D extends C {};
}
