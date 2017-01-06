Utilities.extendObject(SimpleCanvasStage.prototype, {
    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this.offsetIndex = Math.max(this.offsetIndex + count, 0);
            return;
        }

        this.offsetIndex = this.offsetIndex + count;
        if (this.offsetIndex > this.objects.length) {
            // For some tests, it may be easier to see how well the test is going
            // by limiting the range of coordinates in which new objects can reside
            var coordinateMaximumFactor = Math.min(this.objects.length, Math.min(this.size.x, this.size.y)) / Math.min(this.size.x, this.size.y);
            var newIndex = this.offsetIndex - this.objects.length;
            for (var i = 0; i < newIndex; ++i)
                this.objects.push(new this._canvasObject(this, coordinateMaximumFactor));
        }
    },

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        for (var i = 0, length = this.offsetIndex; i < length; ++i)
            this.objects[i].draw(context);
    },

    complexity: function()
    {
        return this.offsetIndex;
    }
});
