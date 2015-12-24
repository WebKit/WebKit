(function() {

// === PAINT OBJECTS ===

function CanvasLineSegment(stage)
{
    var circle = stage.randomInt(0, 2);
    this._color = ["#e01040", "#10c030", "#e05010"][circle];
    this._lineWidth = Math.pow(Math.random(), 12) * 20 + 3;
    this._omega = Math.random() * 3 + 0.2;
    var theta = stage.randomAngle();
    this._cosTheta = Math.cos(theta);
    this._sinTheta = Math.sin(theta);
    this._startX = stage.circleRadius * this._cosTheta + (0.5 + circle) / 3 * stage.size.x;
    this._startY = stage.circleRadius * this._sinTheta + stage.size.y / 2;
    this._length = Math.pow(Math.random(), 8) * 40 + 20;
    this._segmentDirection = Math.random() > 0.5 ? -1 : 1;
}

CanvasLineSegment.prototype.draw = function(context)
{
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;

    this._length+=Math.sin(Date.now()/100*this._omega);

    context.beginPath();
    context.moveTo(this._startX, this._startY);
    context.lineTo(this._startX + this._segmentDirection * this._length * this._cosTheta,
                   this._startY + this._segmentDirection * this._length * this._sinTheta);
    context.stroke();
};

function CanvasArc(stage)
{
    var maxX = 6, maxY = 3;
    var distanceX = stage.size.x / maxX;
    var distanceY = stage.size.y / (maxY + 1);
    var randY = stage.randomInt(0, maxY);
    var randX = stage.randomInt(0, maxX - 1 * (randY % 2));

    this._point = new Point(distanceX * (randX + (randY % 2) / 2), distanceY * (randY + .5));

    this._radius = 20 + Math.pow(Math.random(), 5) * (Math.min(distanceX, distanceY) / 1.6);
    this._startAngle = stage.randomAngle();
    this._endAngle = stage.randomAngle();
    this._omega = (Math.random() - 0.5) * 0.3;
    this._counterclockwise = stage.randomBool();
    var colors = ["#101010", "#808080", "#c0c0c0"];
    colors.push(["#e01040", "#10c030", "#e05010"][(randX + Math.ceil(randY / 2)) % 3]);
    this._color = colors[Math.floor(Math.random() * colors.length)];
    this._lineWidth = 1 + Math.pow(Math.random(), 5) * 30;
    this._doStroke = stage.randomInt(0, 3) != 0;
};

CanvasArc.prototype.draw = function(context)
{
    this._startAngle += this._omega;
    this._endAngle += this._omega / 2;

    if (this._doStroke) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.arc(this._point.x, this._point.y, this._radius, this._startAngle, this._endAngle, this._counterclockwise);
        context.stroke();
    } else {
        context.fillStyle = this._color;
        context.beginPath();
        context.lineTo(this._point.x, this._point.y);
        context.arc(this._point.x, this._point.y, this._radius, this._startAngle, this._endAngle, this._counterclockwise);
        context.lineTo(this._point.x, this._point.y);
        context.fill();
    }
};

function CanvasLinePoint(stage, coordinateMaximum)
{
    var X_LOOPS = 40;
    var Y_LOOPS = 20;

    var offsets = [[-2, -1], [2, 1], [-1, 0], [1, 0], [-1, 2], [1, -2]];
    var offset = offsets[Math.floor(Math.random() * offsets.length)];

    this.coordinate = new Point(X_LOOPS/2, Y_LOOPS/2);
    if (stage._objects.length) {
        var head = stage._objects[stage._objects.length - 1].coordinate;
        this.coordinate.x = head.x;
        this.coordinate.y = head.y;
    }

    var nextCoordinate = this.coordinate.x + offset[0];
    if (nextCoordinate < 0 || nextCoordinate > X_LOOPS)
        this.coordinate.x -= offset[0];
    else
        this.coordinate.x = nextCoordinate;
    nextCoordinate = this.coordinate.y + offset[1];
    if (nextCoordinate < 0 || nextCoordinate > Y_LOOPS)
        this.coordinate.y -= offset[1];
    else
        this.coordinate.y = nextCoordinate;

    var xOff = .25 * (this.coordinate.y % 2);
    var randX = (xOff + this.coordinate.x) * stage.size.x / X_LOOPS;
    var randY = this.coordinate.y * stage.size.y / Y_LOOPS;
    var colors = ["#101010", "#808080", "#c0c0c0", "#e01040"];
    this._color = colors[Math.floor(Math.random() * colors.length)];

    this._width = Math.pow(Math.random(), 5) * 20 + 1;
    this._isSplit = Math.random() > 0.9;
    this._point = new Point(randX, randY);

}

CanvasLinePoint.prototype.draw = function(context, stage)
{
    context.lineTo(this._point.x, this._point.y);
};


// === STAGES ===

function SimpleCanvasPathStrokeStage(element, options, canvasObject)
{
    SimpleCanvasStage.call(this, element, options, canvasObject);
    this.context.lineCap = options["lineCap"] || "butt";
    this.context.lineJoin = options["lineJoin"] || "bevel";
}
SimpleCanvasPathStrokeStage.prototype = Object.create(SimpleCanvasStage.prototype);
SimpleCanvasPathStrokeStage.prototype.constructor = SimpleCanvasPathStrokeStage;
SimpleCanvasPathStrokeStage.prototype.animate = function() {
    var context = this.context;
    var stage = this;

    context.beginPath();
    this._objects.forEach(function(object, index) {
        if (index == 0) {
            context.lineWidth = object._width;
            context.strokeStyle = object._color;
            context.moveTo(object._point.x, object._point.y);
        } else {
            if (object._isSplit) {
                context.stroke();

                context.lineWidth = object._width;
                context.strokeStyle = object._color;
                context.beginPath();
            }

            if (Math.random() > 0.999)
                object._isSplit = !object._isSplit;

            object.draw(context);
        }
    });
    context.stroke();
}

function CanvasLineSegmentStage(element, options)
{
    SimpleCanvasStage.call(this, element, options, CanvasLineSegment);
    this.context.lineCap = options["lineCap"] || "butt";
    this.circleRadius = this.size.x / 3 / 2 - 20;
}
CanvasLineSegmentStage.prototype = Object.create(SimpleCanvasStage.prototype);
CanvasLineSegmentStage.prototype.constructor = CanvasLineSegmentStage;
CanvasLineSegmentStage.prototype.animate = function()
{
    var context = this.context;
    var stage = this;

    context.lineWidth = 30;
    for(var i = 0; i < 3; i++) {
        context.strokeStyle = ["#e01040", "#10c030", "#e05010"][i];
        context.fillStyle = ["#70051d", "#016112", "#702701"][i];
        context.beginPath();
            context.arc((0.5 + i) / 3 * stage.size.x, stage.size.y/2, stage.circleRadius, 0, Math.PI*2);
        context.stroke();
        context.fill();
    }

    this._objects.forEach(function(object) {
        object.draw(context);
    });
}

function CanvasLinePathStage(element, options)
{
    SimpleCanvasPathStrokeStage.call(this, element, options, CanvasLinePoint);
    this.context.lineJoin = options["lineJoin"] || "bevel";
    this.context.lineCap = options["lineCap"] || "butt";
}
CanvasLinePathStage.prototype = Object.create(SimpleCanvasPathStrokeStage.prototype);
CanvasLinePathStage.prototype.constructor = CanvasLinePathStage;

// === BENCHMARK ===

function CanvasPathBenchmark(suite, test, options, progressBar)
{
    SimpleCanvasBenchmark.call(this, suite, test, options, progressBar);
}
CanvasPathBenchmark.prototype = Object.create(SimpleCanvasBenchmark.prototype);
CanvasPathBenchmark.prototype.constructor = CanvasPathBenchmark;
CanvasPathBenchmark.prototype.createStage = function(element)
{
    switch (this._options["pathType"]) {
    case "line":
        return new CanvasLineSegmentStage(element, this._options);
    case "arcs":
        return new SimpleCanvasStage(element, this._options, CanvasArc);
    case "linePath":
        return new CanvasLinePathStage(element, this._options);
    }
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new CanvasPathBenchmark(suite, test, options, progressBar);
}

})();