SimpleCanvasStage = Utilities.createSubclass(Stage,
    function(canvasObject)
    {
        Stage.call(this);
        this._canvasObject = canvasObject;
        this.objects = [];
        this.offsetIndex = 0;
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

        if (count < 0) {
            this.offsetIndex = Math.min(this.offsetIndex - count, this.objects.length);
            return;
        }

        var newIndex = this.offsetIndex - count;
        if (newIndex < 0) {
            this.offsetIndex = 0;
            newIndex = -newIndex;
            for (var i = 0; i < newIndex; ++i) {
                if (this._canvasObject.constructor === Array)
                    this.objects.push(new (Stage.randomElementInArray(this._canvasObject))(this));
                else
                    this.objects.push(new this._canvasObject(this));
            }
        } else
            this.offsetIndex = newIndex;
    },

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        for (var i = this.offsetIndex, length = this.objects.length; i < length; ++i)
            this.objects[i].draw(context);
    },

    complexity: function()
    {
        return this.objects.length - this.offsetIndex;
    }
});
