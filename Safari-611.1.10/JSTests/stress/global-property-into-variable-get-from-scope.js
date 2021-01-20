// This tests a bug that occured because we have noInline as a global
// property then is set to a global variable in the load of standalone-pre.js.
// Once we get to the DFG we still think that noInline is a global property
// based on an old global object structure. This would cause a crash when we
// attempted to create an equivalence condition for the get_from_scope of
// noInline as the current global object would not have noInline as a property
// at that point and we would attempt to access the value at an invalid offset.

load("resources/standalone-pre.js", "caller relative");

noInline();

for (i = 0; i < 100000; i++);
