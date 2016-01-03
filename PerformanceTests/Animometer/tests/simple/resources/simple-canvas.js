Utilities.extendObject(SimpleCanvasStage.prototype, {
    tune: function(count)
    {
        if (count == 0)
            return this.objects.length;

        if (count > 0) {
            // For path-based tests, use the object length as a maximum coordinate value
            // to make it easier to see what the test is doing
            var coordinateMaximum = Math.max(this.objects.length, 200);
            for (var i = 0; i < count; ++i)
                this.objects.push(new this._canvasObject(this, coordinateMaximum));
            return this.objects.length;
        }

        count = Math.min(-count, this.objects.length);
        this.objects.splice(-count, count);
        return this.objects.length;
    }
});
