//@ $skipModes << :lockdown if $buildType == "debug"

class Point {
    constructor(x, y, z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }
}
noInline(Point);

function create(args) {
    return new Point(...args);
}
noInline(create);

const argumentsList = [[], [1], [1, 2], [1, 2, 3], [1.5], [1.5, 2.5, 3.5], [{}, 2], [{}, 2, 3]];
let result = 0;
for (let i = 0; i < 4e6; ++i)
    result += create(argumentsList[i & 7]).y === 2;
if (result !== 2e6)
    throw new Error(result);
