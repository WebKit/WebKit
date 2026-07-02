// If there is no window.gc() already defined, define one using the best
// method we can find.
// The slow fallback should not hit in the actual test environment.
if (!window.gc)
{
    window.gc = function()
    {
        if (window.GCController)
            return GCController.collect();

        console.warn('Tests are running without the ability to do manual garbage collection. They will still work, but coverage will be suboptimal.');
        function gcRec(n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        }
        for (var i = 0; i < 10000; i++)
            gcRec(10);
    }
}

// Fill array with null to avoid potential GC reachability. frames = null can be enough in 99.9% cases,
// but we are extra careful to make tests non-flaky: considering about the case that array is captured
// somewhere (like DFG constants).
function nukeArray(array)
{
    for (let index = 0; index < array.length; ++index)
        array[index] = null;
    array.length = 0;
}

async function testDocumentIsNotLeaked(init, options = {})
{
    let gcCount = options.gcCount ?? 50;
    let documentCount = options.documentCount ?? 20;
    let additionalCheck = options.additionalCheck ?? function () { return true; };

    function waitFor(duration)
    {
        return new Promise((resolve) => setTimeout(resolve, duration));
    }

    let frameIdentifiers = await init(documentCount);
    for (let counter = 0; counter < gcCount; ++counter) {
        for (let i = 0; i < documentCount; ++i) {
            let frameIdentifier = frameIdentifiers[i];
            if (!internals.isDocumentAlive(frameIdentifier) && additionalCheck())
                return "Document did not leak";
        }
        gc();
        await waitFor(50);
    }

    throw new Error("Document is leaked");
}
