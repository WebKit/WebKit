function wait(memory, index, waitCondition, wakeCondition)
{
    while (memory[index] == waitCondition) {
        var result = Atomics.wait(memory, index, waitCondition);
        switch (result) {
        case "not-equal":
        case "ok":
            break;
        default:
            postMessage("Error: bad result from wait: " + result);
            postMessage("error");
            break;
        }
        var value = memory[index];
        if (value != wakeCondition) {
            postMessage("Error: wait returned not-equal but the memory has a bad value: " + value);
            postMessage("error");
        }
    }
    var value = memory[index];
    if (value != wakeCondition) {
        postMessage("Error: done waiting but the memory has a bad value: " + value);
        postMessage("error");
    }
}

function wake(memory, index)
{
    var result = Atomics.wake(memory, index, 1);
    if (result != 0 && result != 1) {
        postMessage("Error: bad result from wake: " + result);
        postMessage("error");
    }
}

function checkBufferSharing(shouldShareBuffer)
{
    var set = new Set();
    for (var i = 1; i < arguments.length; ++i)
        set.add(arguments[i].buffer);
    if (shouldShareBuffer) {
        if (set.size != 1) {
            postMessage("Error: buffers should be shared but are not shared (set.size == " + set.size + ")");
            postMessage("error");
        }
    } else {
        if (set.size != arguments.length - 1) {
            postMessage("Error: buffers should not be shared but are shared");
            postMessage("error");
        }
    }
}

