// This tests that we do not constantly OSR on instanceof where the RHS is a bound function.
// While this bound functions are unlikely to be passed to instanceof often C-API users use
// the same method of overriding instanceof expressions.


function Constructor(x) {
    this.x = x;
}

Constructor.prototype = {}

BoundConstructor = Constructor.bind();
foo = new Constructor(1);
bar = new BoundConstructor(1);

i = 0;

function test()
{
    if (!(foo instanceof BoundConstructor)) {
        throw new Error("foo should be an instanceof BoundConstructor");
    }
    let j = 0;
    for (;j < 1000; j++) {}
    return j;
}
noInline(test);

for (i = 0; i < 50000; i++)
    test();
