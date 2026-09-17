//@ $skipModes << :lockdown if $buildType == "debug"

class Point {
    constructor(x, y)
    {
        this.x = x;
        this.y = y;
    }
}

function create(constructor, ...args)
{
    return Reflect.construct(constructor, args);
}
noInline(create);

var sum = 0;
for (var i = 0; i < 3e6; ++i) {
    var point = create(Point, i, 1);
    sum += point.x + point.y;
}
if (sum !== 4500001500000)
    throw new Error("bad sum: " + sum);
