load("./driver/driver.js");

(function() {
    const bufferBytes = 4 * 2000;
    const typedArraySize = 1000;
    const typedArrayBytes = 4 * typedArraySize;
    assert(typedArrayBytes < bufferBytes, "Sizes should be different");

    let buffer = new ArrayBuffer(bufferBytes);
    let view = new Float32Array(buffer);
    let typedArray = new Uint32Array(typedArraySize);

    let snapshot = createCheapHeapSnapshot();

    let arrayBufferNodes = snapshot.nodesWithClassName("ArrayBuffer");
    let viewNodes = snapshot.nodesWithClassName("Float32Array");
    let typedArrayNodes = snapshot.nodesWithClassName("Uint32Array");
    assert(arrayBufferNodes.length === 1, "Snapshot should contain 1 'ArrayBuffer' instance");
    assert(viewNodes.length === 1, "Snapshot should contain 1 'Float32Array' instance");
    assert(typedArrayNodes.length === 1, "Snapshot should contain 1 'Uint32Array' instance");

    let arrayBufferNode = arrayBufferNodes[0];
    let viewNode = viewNodes[0];
    let typedArrayNode = typedArrayNodes[0];
    assert(arrayBufferNode.size >= bufferBytes, "ArrayBuffer node should have a large size");
    assert(viewNode.size <= 100, "Float32Array node should have a very small size, it just wraps the already large ArrayBuffer");
    assert(typedArrayNode.size >= typedArrayBytes && typedArrayNode.size < bufferBytes, "Uint32Array node should have a large size, but not as large as the ArrayBuffer");
})();
