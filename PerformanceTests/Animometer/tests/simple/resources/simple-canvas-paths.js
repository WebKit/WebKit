// === PAINT OBJECTS ===

function CanvasQuadraticSegment(stage) {
    var maxSize = stage.randomInt(20, 200);
    var toCenter = stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

    this._point1 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point2 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point3 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._color = stage.randomColor();
    this._lineWidth = stage.randomInt(1, 50);
};
CanvasQuadraticSegment.prototype.draw = function(context) {
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;
    context.beginPath();
    context.moveTo(this._point1.x, this._point1.y);
    context.quadraticCurveTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y);
    context.stroke();
};

function CanvasBezierSegment(stage) {
    var maxSize = stage.randomInt(20, 200);
    var toCenter = stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

    this._point1 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point2 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point3 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point4 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._color = stage.randomColor();
    this._lineWidth = stage.randomInt(1, 50);
};
CanvasBezierSegment.prototype.draw = function(context) {
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;
    context.beginPath();
    context.moveTo(this._point1.x, this._point1.y);
    context.bezierCurveTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y, this._point4.x, this._point4.y);
    context.stroke();
};

function CanvasArcToSegment(stage) {
    var maxSize = stage.randomInt(20, 200);
    var toCenter = stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

    this._point1 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point2 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._point3 = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._radius = stage.randomInt(20, 200);
    this._color = stage.randomColor();
    this._lineWidth = stage.randomInt(1, 50);
};
CanvasArcToSegment.prototype.draw = function(context) {
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;
    context.beginPath();
    context.moveTo(this._point1.x, this._point1.y);
    context.arcTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y, this._radius);
    context.stroke();
};

function CanvasArcSegment(stage) {
    var maxSize = stage.randomInt(20, 200);
    var toCenter = stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

    this._point = stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
    this._radius = stage.randomInt(20, 200);
    this._startAngle = stage.randomAngle();
    this._endAngle = stage.randomAngle();
    this._counterclockwise = stage.randomBool();
    this._color = stage.randomColor();
    this._lineWidth = stage.randomInt(1, 50);
};
CanvasArcSegment.prototype.draw = function(context) {
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;
    context.beginPath();
    context.arc(this._point.x, this._point.y, this._radius, this._startAngle, this._endAngle, this._counterclockwise);
    context.stroke();
};

function CanvasRect(stage) {
    this._width = stage.randomInt(20, 200);
    this._height = stage.randomInt(20, 200);
    this._point = stage.randomPosition(stage.size).subtract(new Point(this._width/2, this._height/2));
    this._color = stage.randomColor();
    this._lineWidth = stage.randomInt(1, 20);
}
CanvasRect.prototype.draw = function(context) {
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;
    context.beginPath();
    context.rect(this._point.x, this._point.y, this._width, this._height);
    context.stroke();
};

function CanvasRectFill(stage) {
    CanvasRect.call(this, stage);
}
CanvasRectFill.prototype.draw = function(context) {
    context.fillStyle = this._color;
    context.beginPath();
    context.rect(this._point.x, this._point.y, this._width, this._height);
    context.fill();
};

// === BENCHMARK ===

function CanvasPathBenchmark(suite, test, options, recordTable, progressBar) {
    SimpleCanvasBenchmark.call(this, suite, test, options, recordTable, progressBar);
}
CanvasPathBenchmark.prototype = Object.create(SimpleCanvasBenchmark.prototype);
CanvasPathBenchmark.prototype.constructor = CanvasPathBenchmark;
CanvasPathBenchmark.prototype.createStage = function(element)
{
    switch (this._options["pathType"]) {
    case "quadratic":
        return new SimpleCanvasStage(element, this._options, CanvasQuadraticSegment);
    case "bezier":
        return new SimpleCanvasStage(element, this._options, CanvasBezierSegment);
    case "arcTo":
        return new SimpleCanvasStage(element, this._options, CanvasArcToSegment);
    case "arc":
        return new SimpleCanvasStage(element, this._options, CanvasArcSegment);
    case "rect":
        return new SimpleCanvasStage(element, this._options, CanvasRect);
    case "arcToFill":
        return new SimpleCanvasStage(element, this._options, CanvasArcToSegmentFill);
    case "arcFill":
        return new SimpleCanvasStage(element, this._options, CanvasArcSegmentFill);
    case "rectFill":
        return new SimpleCanvasStage(element, this._options, CanvasRectFill);
    }
}

window.benchmarkClient.create = function(suite, test, options, recordTable, progressBar) {
    return new CanvasPathBenchmark(suite, test, options, recordTable, progressBar);
}
