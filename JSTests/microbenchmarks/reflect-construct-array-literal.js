//@ $skipModes << :lockdown if $buildType == "debug"

function Point(x, y)
{
    this.x = x;
    this.y = y;
}

function test(x, y)
{
    return Reflect.construct(Point, [x, y]);
}
noInline(test);

var sum = 0;
for (var i = 0; i < 3e6; ++i) {
    var point = test(i, 1);
    sum += point.x + point.y;
}
if (sum !== 4500001500000)
    throw new Error("bad sum: " + sum);
