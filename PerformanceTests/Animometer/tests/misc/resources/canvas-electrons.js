(function() {

function CanvasElectron(stage)
{
    this._context = stage.context;
    this._stageSize = stage.size;

    var minSide = Math.min(this._stageSize.width, this._stageSize.height);
    var radiusX = Stage.random(minSide / 8, 7 * minSide / 16);
    var radiusY = Stage.random(minSide / 8, 3 * radiusX / 4);
    this._orbitRadiuses = new Point(radiusX, radiusY);
    this._radius = Stage.random(5, 15);
    this._direction = Stage.randomInt(0, 2);
    this._angle = Stage.randomInt(0, 360);
    this._color = Stage.randomColor();
    this._rotater = Stage.randomRotater();
    this._rotater.next(Stage.random(0, this._rotater.interval));
}

CanvasElectron.prototype = {
    _draw: function()
    {
        // Calculate the position of the object on the ellipse.
        var angle = this._direction ? this._rotater.degree() : 360 - this._rotater.degree();
        var position = this._stageSize.center.subtract(Point.pointOnEllipse(angle, this._orbitRadiuses));

        this._context.save();
            this._context.translate(this._stageSize.center.x, this._stageSize.center.y);
            this._context.rotate(this._angle * Math.PI / 180);
            this._context.translate(-this._stageSize.center.x, -this._stageSize.center.y);

            // Set the stroke and the fill colors
            this._context.strokeStyle = "rgba(192, 192, 192, 0.9)";
            this._context.fillStyle = this._color;

            // Draw the orbit of the object.
            if (this._context.ellipse) {
                this._context.beginPath();
                this._context.ellipse(this._stageSize.center.x, this._stageSize.center.y, this._orbitRadiuses.x, this._orbitRadiuses.y, 0, 0, 2 * Math.PI);
                this._context.stroke();
            }

            // Draw the object.
            this._context.beginPath();
            this._context.arc(position.x, position.y, this._radius, 0, Math.PI * 2, true);
            this._context.fill();
        this._context.restore();
    },

    animate: function(timeDelta)
    {
        this._rotater.next(timeDelta / 100);
        this._draw();
    }
};

CanvasElectronsStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
        this._electrons = [];
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
                this._electrons.push(new CanvasElectron(this));
            return;
        }

        count = Math.min(-count, this._electrons.length);
        this._electrons.splice(-count, count);
    },

    animate: function(timeDelta)
    {
        this.context.clearRect(0, 0, this.size.x, this.size.y);

        // Draw a big star in the middle.
        this.context.fillStyle = "orange";
        this.context.beginPath();
        this.context.arc(this.size.center.x, this.size.center.y, 50, 0, Math.PI * 2, true);
        this.context.fill();

        this._electrons.forEach(function(electron) {
            electron.animate(timeDelta);
        });
    },

    complexity: function()
    {
        return this._electrons.length;
    }
});

CanvasElectronsBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new CanvasElectronsStage(), options);
    }
);

window.benchmarkClass = CanvasElectronsBenchmark;

})();