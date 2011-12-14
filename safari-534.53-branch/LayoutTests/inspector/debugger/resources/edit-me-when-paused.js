function f2()
{
    return 1;
}

function f1()
{
    var x = 1;
    debugger;
    return x + f2();
}
