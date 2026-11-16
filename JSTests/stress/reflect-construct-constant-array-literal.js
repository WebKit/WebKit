function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

class Point {
    constructor(x, y)
    {
        this.x = x;
        this.y = y;
        this.count = arguments.length;
        this.newTarget = new.target;
    }
}

function Other() { }

function int32Literal()
{
    return Reflect.construct(Point, [1, 2]);
}
noInline(int32Literal);

function doubleLiteral()
{
    return Reflect.construct(Point, [1.5, -0]);
}
noInline(doubleLiteral);

function mixedLiteral()
{
    return Reflect.construct(Point, [1, "two", null]);
}
noInline(mixedLiteral);

function withNewTarget(newTarget)
{
    return Reflect.construct(Point, [1, 2], newTarget);
}
noInline(withNewTarget);

function storedBeforeCall()
{
    let array = [1, 2];
    array[0] = 9;
    return Reflect.construct(Point, array);
}
noInline(storedBeforeCall);

function pushedBeforeCall()
{
    let array = [1, 2];
    array.push(3);
    return Reflect.construct(Point, array);
}
noInline(pushedBeforeCall);

function usedAfterward()
{
    let array = [1, 2];
    let point = Reflect.construct(Point, array);
    array.push(point.x);
    return array;
}
noInline(usedAfterward);

for (let i = 0; i < testLoopCount; ++i) {
    let point = int32Literal();
    shouldBe(point.x, 1);
    shouldBe(point.y, 2);
    shouldBe(point.count, 2);
    shouldBe(point.newTarget, Point);

    point = doubleLiteral();
    shouldBe(point.x, 1.5);
    shouldBe(Object.is(point.y, -0), true);

    point = mixedLiteral();
    shouldBe(point.x, 1);
    shouldBe(point.y, "two");
    shouldBe(point.count, 3);

    point = withNewTarget(i & 1 ? Other : Point);
    shouldBe(point.x, 1);
    shouldBe(point.newTarget, i & 1 ? Other : Point);
    shouldBe(Object.getPrototypeOf(point), i & 1 ? Other.prototype : Point.prototype);

    point = storedBeforeCall();
    shouldBe(point.x, 9);
    shouldBe(point.y, 2);

    point = pushedBeforeCall();
    shouldBe(point.count, 3);

    let array = usedAfterward();
    shouldBe(array.length, 3);
    shouldBe(array[2], 1);
}
