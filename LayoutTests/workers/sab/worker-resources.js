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

var cascadeLockUnlocked = 0;
var cascadeLockLocked = 1;
var cascadeLockLockedAndParked = 2;

function cascadeLockSlow(memory, index)
{
    var desiredState = cascadeLockLocked;
    for (;;) {
        if (Atomics.compareExchange(memory, index, cascadeLockUnlocked, desiredState) == cascadeLockUnlocked)
            return;
        
        desiredState = cascadeLockLockedAndParked;
        Atomics.compareExchange(memory, index, cascadeLockLocked, cascadeLockLockedAndParked);
        Atomics.wait(memory, index, cascadeLockLockedAndParked);
    }
}

function cascadeLock(memory, index)
{
    if (Atomics.compareExchange(memory, index, cascadeLockUnlocked, cascadeLockLocked) == cascadeLockUnlocked)
        return;
    
    cascadeLockSlow(memory, index);
}

function cascadeUnlock(memory, index)
{
    if (Atomics.exchange(memory, index, cascadeLockUnlocked) == cascadeLockLocked)
        return;

    Atomics.wake(memory, index, 1);
}

