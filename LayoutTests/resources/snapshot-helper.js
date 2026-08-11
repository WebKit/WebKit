
window.SnapshotHelper = class SnapshotHelper {
    static async takeSnapshot(node)
    {
        const imageData = internals.snapshotNode(node);

        if (!imageData) {
            console.log('FAIL: snapshotNode returned null');
            return;
        }

        const canvas = document.createElement('canvas');
        canvas.width = imageData.width;
        canvas.height = imageData.height;
        canvas.getContext('2d').putImageData(imageData, 0, 0);

        return canvas;
    }
}
