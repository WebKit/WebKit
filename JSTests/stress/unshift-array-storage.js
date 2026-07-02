// FIXME: Bring back something like the deferGC probability mode.
// https://bugs.webkit.org/show_bug.cgi?id=166627
//@ skip
// //@ runFTLNoCJIT("--scribbleFreeCells=true", "--deferGCShouldCollectWithProbability=true", "--deferGCProbability=1")

// Create some array storage.
var array = [];
ensureArrayStorage(array);

for (var i = 0; i < 1000; ++i)
    array.push(i);

array.unshift(1, 2, 3, 4); // This will crash if we aren't careful about when we GC during unshift.
