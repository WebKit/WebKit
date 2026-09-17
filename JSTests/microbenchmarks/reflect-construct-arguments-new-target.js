//@ $skipModes << :lockdown if $buildType == "debug"

function Base(x, y)
{
    this.x = x;
    this.y = y;
}

function Derived()
{
    return Reflect.construct(Base, arguments, new.target);
}
Derived.prototype = Object.create(Base.prototype);

function test(x, y)
{
    return new Derived(x, y);
}
noInline(test);

var sum = 0;
for (var i = 0; i < 3e6; ++i) {
    var object = test(i, 1);
    if (Object.getPrototypeOf(object) !== Derived.prototype)
        throw new Error("bad prototype");
    sum += object.x + object.y;
}
if (sum !== 4500001500000)
    throw new Error("bad sum: " + sum);
