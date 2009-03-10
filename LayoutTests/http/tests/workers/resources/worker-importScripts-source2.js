if (!loadedSource1)
    postMessage("Resource 2 loaded before resource 1");
loadedSource2 = true;
postMessage("Loaded resource 2");
if (this.secondShouldThrow) {
    postMessage("Second resource throwing an exception");
    throw "Thrown by second resource";
}
