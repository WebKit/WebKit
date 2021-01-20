{
    let functions = [];
    for (var i = 0; i < 1e5; ++i)
        functions.push($vm.createEmptyFunctionWithName(i));
    let newGlobal = $vm.createGlobalObject();
    newGlobal.WeakMap.prototype.set;
    newGlobal.WeakMap.prototype.set = null; // Remove reference!
    $vm.gcSweepAsynchronously(); // Mark WeakMap#set dead, but since we have so many NativeExecutables, it is dead, but still in JITThunks.
    newGlobal = $vm.createGlobalObject();
    let set = newGlobal.WeakMap.prototype.set; // Accessing to HashTable, which found a dead previous NativeExecutable, and replace it with new one.
    $vm.gc(); // This does not invoke finalizer for WeakMap#set since we already replaced it. And previous one is finally destroyed.
    try {
        set(); // Of course, it works. It is using a new NativeExecutable for WeakMap#set, not using dead one.
    } catch { };
    for (var i = 0; i < 1e3; ++i)
        functions.push($vm.createEmptyFunctionWithName(i));
    try {
        set();
    } catch { };
}
