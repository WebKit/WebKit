// FIXME: Remove this file when implemented in WebCore.

function CountQueuingStrategy({ highWaterMark }) {
    createDataProperty(this, 'highWaterMark', highWaterMark);
}

CountQueuingStrategy.prototype = {
    size: function(chunk) {
        return 1;
    }
}
