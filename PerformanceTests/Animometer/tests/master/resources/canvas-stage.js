SimpleCanvasStage = Utilities.createSubclass(Stage,
    function(canvasObject)
    {
        Stage.call(this);
        this._canvasObject = canvasObject;
        this.objects = [];
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this.context = this.element.getContext("2d");
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count > 0) {
            for (var i = 0; i < count; ++i)
                this.objects.push(new this._canvasObject(this));
            return;
        }

        count = Math.min(-count, this.objects.length);
        this.objects.splice(0, count);
    },

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        this.objects.forEach(function(object) {
            object.draw(context);
        });
    },

    complexity: function()
    {
        return this.objects.length;
    }
});
