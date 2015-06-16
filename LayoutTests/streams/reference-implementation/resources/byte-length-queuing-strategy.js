// FIXME: Remove this file when implemented in WebCore.

function ByteLengthQueuingStrategy({ highWaterMark }) {
    createDataProperty(this, 'highWaterMark', highWaterMark);
}

ByteLengthQueuingStrategy.prototype = {
    size: function(chunk) {
        return chunk.byteLength;
    }
}
