loadedSource1 = true;
postMessage("Loaded resource 1");
if (this.firstShouldThrow) {
    postMessage("First resource throwing an exception");
    throw "Thrown by first resource"
}
