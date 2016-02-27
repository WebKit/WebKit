(function() {

var maxVerticalOffset = 50;
var radius = 20;
var sizeVariance = 80;
var travelDistance = 70;

var minObjectDepth = 0.2;
var maxObjectDepth = 1.0;

var opacityMultiplier = 40;

var FocusElement = Utilities.createClass(
    function(stage)
    {
        var topOffset = maxVerticalOffset * Stage.randomSign();
        var top = Stage.random(0, stage.size.height - 2 * radius - sizeVariance);
        var left = Stage.random(0, stage.size.width - 2 * radius - sizeVariance);

        // size and blurring are a function of depth
        this._depth = Utilities.lerp(1 - Math.pow(Pseudo.random(), 2), minObjectDepth, maxObjectDepth);
        var distance = Utilities.lerp(this._depth, 1, sizeVariance);
        var size = 2 * radius + sizeVariance - distance;

        this.element = document.createElement('div');
        this.element.style.width = size + "px";
        this.element.style.height = size + "px";
        this.element.style.top = top + "px";
        this.element.style.left = left + "px";
        this.element.style.zIndex = Math.round((1 - this._depth) * 10);

        Utilities.setElementPrefixedProperty(this.element, "filter", "blur(" + stage.getBlurValue(this._depth) + "px) opacity(" + stage.getOpacityValue(this._depth) + "%)");

        var depthMultiplier = Utilities.lerp(1 - this._depth, 0.8, 1);
        this._sinMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier;
        this._cosMultiplier = Pseudo.random() * Stage.randomSign() * depthMultiplier;
    }, {

    animate: function(stage, sinTime, cosTime)
    {
        var top = sinTime * this._sinMultiplier * travelDistance;
        var left = cosTime * this._cosMultiplier * travelDistance;

        Utilities.setElementPrefixedProperty(this.element, "filter", "blur(" + stage.getBlurValue(this._depth) + "px) opacity(" + stage.getOpacityValue(this._depth) + "%)");
        this.element.style.transform = "translateX(" + left + "%) translateY(" + top + "%)";
    }
});

var FocusStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    movementDuration: 2500,
    focusDuration: 1000,

    centerObjectDepth: 0.0,

    minBlurValue: 1.5,
    maxBlurValue: 15,
    maxCenterObjectBlurValue: 5,

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);

        this._testElements = [];
        this._focalPoint = 0.5;

        this._centerElement = document.getElementById("center-text");
        this._centerElement.style.width = (radius * 5) + 'px';
        this._centerElement.style.height = (radius * 5) + 'px';
        this._centerElement.style.zIndex = Math.round(10 * this.centerObjectDepth);

        var blur = this.getBlurValue(this.centerObjectDepth);
        Utilities.setElementPrefixedProperty(this._centerElement, "filter", "blur(" + blur + "px)");
    },

    complexity: function()
    {
        return 1 + this._testElements.length;
    },

    tune: function(count)
    {
        if (count == 0)
            return;

        if (count > 0) {
            for (var i = 0; i < count; ++i) {
                var obj = new FocusElement(this);
                this._testElements.push(obj);
                this.element.appendChild(obj.element);
            }
            return;
        }

        while (count < 0) {
            var obj = this._testElements.shift();
            if (!obj)
                return;

            this.element.removeChild(obj.element);
            count++;
        }
    },

    animate: function()
    {
        var time = this._benchmark.timestamp;
        var sinTime = Math.sin(time / this.movementDuration);
        var cosTime = Math.cos(time / this.movementDuration);

        var focusProgress = Utilities.progressValue(Math.sin(time / this.focusDuration), -1, 1);
        this._focalPoint = focusProgress;

        // update center element before loop
        Utilities.setElementPrefixedProperty(this._centerElement, "filter", "blur(" + this.getBlurValue(this.centerObjectDepth, true) + "px)");

        this._testElements.forEach(function(element) {
            element.animate(this, sinTime, cosTime);
        }, this);
    },

    getBlurValue: function(depth, isCenter)
    {
        var value = Math.abs(depth - this._focalPoint);

        if (isCenter)
            return this.maxCenterObjectBlurValue * value;

        return Utilities.lerp(value, this.minBlurValue, this.maxBlurValue);
    },

    getOpacityValue: function(depth)
    {
        return opacityMultiplier * (1 - Math.abs(depth - this._focalPoint));
    },
});

var FocusBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new FocusStage(), options);
    }
);

window.benchmarkClass = FocusBenchmark;

}());
