function call(site)
{
    return site;
}

function test()
{
    return call`Cocoa`;
}

var first = test();
$vm.deleteAllCodeWhenIdle();
fullGC();
