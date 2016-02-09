(function() {

function CanvasStar(stage)
{
    this._context = stage.context;

    this._size = Stage.randomSquareSize(5, 20);
    this._center = Stage.randomPosition(stage.size.subtract(this._size)).add(this._size.center);
    this._rotateX = 0;
    this._rotateDeltaX = Stage.random(0.3, 0.7);
}

CanvasStar.prototype = {
    _draw: function()
    {
        this._context.save();
        this._context.translate(this._center.x, this._center.y);

        this._context.fillStyle = 'yellow';
        this._context.strokeStyle = 'white';

        this._context.lineWidth = 1;

        this._context.beginPath();
        this._context.moveTo(                                     0, -this._size.y /  2);
        this._context.lineTo(+this._size.x / 20 - this._rotateX / 5, -this._size.y / 10);
        this._context.lineTo(+this._size.x / 4  - this._rotateX,                      0);
        this._context.lineTo(+this._size.x / 20 - this._rotateX / 5, +this._size.y / 10);
        this._context.lineTo(                                     0, +this._size.y /  2);
        this._context.lineTo(-this._size.x / 20 + this._rotateX / 5, +this._size.y / 10);
        this._context.lineTo(-this._size.x /  4 + this._rotateX,                      0);
        this._context.lineTo(-this._size.x / 20 + this._rotateX / 5, -this._size.y / 10);
        this._context.lineTo(                                     0, -this._size.y /  2);

        this._context.fill();
        this._context.stroke();
        this._context.closePath();
        this._context.restore();
    },

    animate: function(timeDelta)
    {
        this._rotateX += this._rotateDeltaX;

        if (this._rotateX > this._size.x / 4 || this._rotateX < -this._size.x / 4) {
            this._rotateDeltaX = -this._rotateDeltaX;
            this._rotateX += this._rotateDeltaX;
        }

        this._draw();
    }
};

CanvasStarsStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
        this._objects = [];
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
                this._objects.push(new CanvasStar(this));
            return;
        }

        count = Math.min(-count, this._objects.length);
        this._objects.splice(-count, count);
    },

    animate: function(timeDelta)
    {
        this.context.beginPath();
        this.context.fillStyle = 'black';
        this.context.rect(0, 0, this.size.width, this.size.height);
        this.context.fill();

        this._objects.forEach(function(object) {
            object.animate(timeDelta);
        });
    },

    complexity: function()
    {
        return this._objects.length;
    }
});

CanvasStarsBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new CanvasStarsStage(), options);
    }
);

window.benchmarkClass = CanvasStarsBenchmark;

})();