Utilities.extendObject(SimpleCanvasStage.prototype, {
    tune: function(count)
    {
        if (count == 0)
            return;

        if (count > 0) {
            // For some tests, it may be easier to see how well the test is going
            // by limiting the range of coordinates in which new objects can reside
            var coordinateMaximumFactor = Math.min(this.objects.length, Math.min(this.size.x, this.size.y)) / Math.min(this.size.x, this.size.y);
            for (var i = 0; i < count; ++i)
                this.objects.push(new this._canvasObject(this, coordinateMaximumFactor));
            return;
        }

        count = Math.min(-count, this.objects.length);
        this.objects.splice(-count, count);
    }
});
