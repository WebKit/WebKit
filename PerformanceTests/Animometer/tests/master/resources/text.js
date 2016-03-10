(function() {

var TextStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);

        this.testElements = [];
        this._offsetIndex = 0;
    }, {

    shadowFalloff: new UnitBezier(new Point(0.015, 0.750), new Point(0.755, 0.235)),
    shimmerAverage: 0.4,
    shimmerMax: 0.5,
    millisecondsPerRotation: 1000 / (.26 * Math.PI * 2),
    particleDistance: 1,
    lightnessMin: 13,
    lightnessMax: 94,

    initialize: function(benchmark)
    {
        Stage.prototype.initialize.call(this, benchmark);

        this._template = document.getElementById("template");
        this._offset = this.size.subtract(Point.elementClientSize(this._template)).multiply(.5);
        this._template.style.left = this._offset.width + "px";
        this._template.style.top = this._offset.height + "px";

        this._stepProgress = 0;
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this._offsetIndex = Math.max(this._offsetIndex + count, 0);
            for (var i = this._offsetIndex; i < this.testElements.length; ++i)
                this.testElements[i].style.visibility = "hidden";

            this._stepProgress = 1 / this._offsetIndex;
            return;
        }

        this._offsetIndex = this._offsetIndex + count;
        this._stepProgress = 1 / this._offsetIndex;

        var index = Math.min(this._offsetIndex, this.testElements.length);
        for (var i = 0; i < index; ++i)
            this.testElements[i].style.visibility = "visible";

        if (this._offsetIndex <= this.testElements.length)
            return;

        for (var i = this.testElements.length; i < this._offsetIndex; ++i) {
            var clone = this._template.cloneNode(true);
            this.testElements.push(clone);
            this.element.insertBefore(clone, this.element.firstChild);
        }
    },

    animate: function(timeDelta) {
        var angle = Stage.dateCounterValue(this.millisecondsPerRotation);

        var x = 0;
        var y = 0;
        var progress = 0;
        var stepX = Math.sin(angle) * this.particleDistance;
        var stepY = Math.cos(angle) * this.particleDistance;
        for (var i = 0; i < this._offsetIndex; ++i) {
            x += stepX;
            y += stepY;

            var element = this.testElements[i];

            var colorProgress = this.shadowFalloff.solve(progress);
            var shimmer = Math.sin(Stage.dateCounterValue(100) - colorProgress);
            colorProgress += Utilities.lerp(shimmer, this.shimmerAverage, this.shimmerMax);
            var interpolatedLightness = Math.round(Utilities.lerp(Math.max(Math.min(colorProgress, 1), 0), this.lightnessMin, this.lightnessMax));

            element.style.color = "hsl(0,0%," + interpolatedLightness + "%)";
            element.style.transform = "translateX(" + Math.floor(y) + "px) translateY(" + Math.floor(x) + "px)";

            progress += this._stepProgress;
        }
    },

    complexity: function()
    {
        return 1 + this._offsetIndex;
    }
});

var TextBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new TextStage(), options);
    }
);

window.benchmarkClass = TextBenchmark;

}());
