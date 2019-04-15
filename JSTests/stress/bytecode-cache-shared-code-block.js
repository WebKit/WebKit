//@ runBytecodeCache

var program = `(function () {
    function a() { }
    function b() { }
    return { a, b };
})`;

loadString(program)().a();
loadString(program)().b();
