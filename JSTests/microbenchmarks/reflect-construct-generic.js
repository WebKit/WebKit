//@ $skipModes << :lockdown if $buildType == "debug"

class Point {
    constructor(x, y)
    {
        this.x = x;
        this.y = y;
    }
}

function construct(target, args, newTarget)
{
    return Reflect.construct(target, args, newTarget);
}
noInline(construct);

var args = [1, 2];
var sum = 0;
for (var i = 0; i < 3e6; ++i) {
    args[0] = i;
    var point = construct(Point, args, Point);
    sum += point.x + point.y;
}
if (sum !== 4500004500000)
    throw new Error("bad sum: " + sum);
