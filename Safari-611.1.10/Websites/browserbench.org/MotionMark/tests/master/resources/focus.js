(function() {

var minimumDiameter = 30;
var sizeVariance = 20;
var travelDistance = 50;

var minBlurValue = 1;
var maxBlurValue = 10;

var opacityMultiplier = 30;
var focusDuration = 1000;
var movementDuration = 2500;

var FocusElement = Utilities.createClass(
    function(stage)
    {
        var size = minimumDiameter + sizeVariance;

        // Size and blurring are a function of depth.
        this._depth = Pseudo.random();
        var distance = Utilities.lerp(this._depth, 0, sizeVariance);
        size -= distance;

        var top = Stage.random(0, stage.size.height - size);
        var left = Stage.random(0, stage.size.width - size);

        this.particle = document.createElement("div");
        this.particle.style.width = size + "px";
        this.particle.style.height = size + "px";
        this.particle.style.top = top + "px";
        this.particle.style.left = left + "px";
        this.particle.style.zIndex = Math.round((1 - this._depth) * 10);

        var depthMultiplier = Utilities.lerp(1 - this._depth, 0.8, 1);
        this._sinMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier * travelDistance;
        this._cosMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier * travelDistance;

        this.animate(stage, 0, 0);
    }, {

    hide: function()
    {
        this.particle.style.display = "none";
    },

    show: function()
    {
        this.particle.style.display = "block";
    },

    animate: function(stage, sinFactor, cosFactor)
    {
        var top = sinFactor * this._sinMultiplier;
        var left = cosFactor * this._cosMultiplier;
        var distance = Math.abs(this._depth - stage.focalPoint);
        var blur = Utilities.lerp(distance, minBlurValue, maxBlurValue);
        var opacity = Math.max(5, opacityMultiplier * (1 - distance));

        Utilities.setElementPrefixedProperty(this.particle, "filter", "blur(" + blur + "px) opacity(" + opacity + "%)");
        this.particle.style.transform = "translate3d(" + left + "%, " + top + "%, 0)";
    }
});

var FocusStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);

        this._testElements = [];
        this._offsetIndex = 0;
        this.focalPoint = 0.5;
    },

    complexity: function()
    {
        return this._offsetIndex;
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this._offsetIndex = Math.max(0, this._offsetIndex + count);
            for (var i = this._offsetIndex; i < this._testElements.length; ++i)
                this._testElements[i].hide();
            return;
        }

        var newIndex = this._offsetIndex + count;
        for (var i = this._testElements.length; i < newIndex; ++i) {
            var obj = new FocusElement(this);
            this._testElements.push(obj);
            this.element.appendChild(obj.particle);
        }
        for (var i = this._offsetIndex; i < newIndex; ++i)
            this._testElements[i].show();
        this._offsetIndex = newIndex;
    },

    animate: function()
    {
        var time = this._benchmark.timestamp;
        var sinFactor = Math.sin(time / movementDuration);
        var cosFactor = Math.cos(time / movementDuration);

        this.focalPoint = 0.5 + 0.5 * Math.sin(time / focusDuration);

        for (var i = 0; i < this._offsetIndex; ++i)
            this._testElements[i].animate(this, sinFactor, cosFactor);
    }
});

var FocusBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new FocusStage(), options);
    }
);

window.benchmarkClass = FocusBenchmark;

}());
