var o = {};
o.__defineSetter__("foo", Array);

function foo()
{
    o.foo = 42;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i)
    foo();

