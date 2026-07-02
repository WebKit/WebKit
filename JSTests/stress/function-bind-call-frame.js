function test(a, b, c)
{
    $vm.assertFrameAligned();
    return a + b + c;
}

{
    var bound = test.bind(undefined, 0, 1, 2);
    function test1() {
        return bound();
    }
    noInline(test1);

    for (let i = 0; i < 1e4; ++i)
        test1();
}
