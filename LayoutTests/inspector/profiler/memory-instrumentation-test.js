var initialize_MemoryInstrumentationTest = function() {

InspectorTest._memoryBlockSize = function(path, root)
{
    var pathPos = 0;
    var children = [root];

    while (true) {
        var name = path[pathPos++];
        var child = null;
        for (var i = 0; i < children.length; i++) {
            if (children[i].name === name) {
                if (pathPos === path.length)
                    return children[i].size;
                else {
                    child = children[i];
                    break;
                }
            }
        }
        if (child) {
            children = child.children;
            if (!children) {
                InspectorTest.addResult(name + " has no children");
                return -1;
            }
        } else {
            InspectorTest.addResult(name + " not found");
            return -1;
        }
    }
    return -1;
};

InspectorTest.validateMemoryBlockSize = function(path, expectedMinimalSize)
{
    function didReceiveMemorySnapshot(error, memoryBlock)
    {
        var size = InspectorTest._memoryBlockSize(path, memoryBlock);
        if (size > expectedMinimalSize)
            InspectorTest.addResult("PASS: block size for path = [" + path.join(", ") + "] is OK.");
        else {
            InspectorTest.addResult("FAIL: block size for path = [" + path.join(", ") + "] is too small.");
            InspectorTest.addResult("expected minimal block size is " + expectedMinimalSize + " actual is " + size);
        }
        InspectorTest.completeTest();
    }

    MemoryAgent.getProcessMemoryDistribution(didReceiveMemorySnapshot.bind(this));
};

};
