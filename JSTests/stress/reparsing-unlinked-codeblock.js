//@ runDefault("--forceCodeBlockToJettisonDueToOldAge=1", "--useUnlinkedCodeBlockJettisoning=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function hello()
{
    return (function () {
        function world() {
            return 42;
        };
        return world();
    }());
}

// Compile hello and world function.
shouldBe(hello(), 42);
// Kick full GC 20 times to make UnlinkedCodeBlock aged and destroyed. Jettison hello CodeBlock, and underlying world UnlinkedCodeBlock.
for (var i = 0; i < 20; ++i)
    fullGC();
// Recompile world.
shouldBe(hello(), 42);
