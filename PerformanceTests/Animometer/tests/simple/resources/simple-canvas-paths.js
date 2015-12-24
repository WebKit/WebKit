// === PAINT OBJECTS ===

function CanvasLineSegment(stage) {
    var radius = stage.randomInt(10, 100);
    var center = stage.randomPosition(stage.size);
    var delta = Point.pointOnCircle(stage.randomAngle(), radius/2);

    this._point1 = center.add(delta);
    this._point2 = center.subtract(delta);
    this._color = stage.randomColor();
    this._lineWidth = stage.randomInt(1, 100);
}
CanvasLineSegment.prototype.draw = function(context) {
    context.strokeStyle = this._color;
    context.lineWidth = this._lineWidth;
    context.beginPath();
    context.moveTo(this._point1.x, this._point1.y);
    context.lineTo(this._point2.x, this._point2.y);
    context.stroke();
};

function CanvasLinePoint(stage, coordinateMaximum) {
    this._point = stage.randomPosition(new Point(Math.min(stage.size.x, coordinateMaximum), Math.min(stage.size.y, coordinateMaximum)));
}
CanvasLinePoint.prototype.draw = function(context) {
    context.lineTo(this._point.x, this._point.y);
};

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

function CanvasQuadraticPoint(stage, coordinateMaximum) {
    var pointMaximum = new Point(Math.min(stage.size.x, coordinateMaximum), Math.min(stage.size.y, coordinateMaximum));
    this._point1 = stage.randomPosition(pointMaximum);
    this._point2 = stage.randomPosition(pointMaximum);
};
CanvasQuadraticPoint.prototype.draw = function(context) {
    context.quadraticCurveTo(this._point1.x, this._point1.y, this._point2.x, this._point2.y);
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

function CanvasBezierPoint(stage, coordinateMaximum) {
    var pointMaximum = new Point(Math.min(stage.size.x, coordinateMaximum), Math.min(stage.size.y, coordinateMaximum));
    this._point1 = stage.randomPosition(pointMaximum);
    this._point2 = stage.randomPosition(pointMaximum);
    this._point3 = stage.randomPosition(pointMaximum);
};
CanvasBezierPoint.prototype.draw = function(context) {
    context.bezierCurveTo(this._point1.x, this._point1.y, this._point2.x, this._point2.y, this._point3.x, this._point3.y);
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

function CanvasArcToSegmentFill(stage) {
    CanvasArcToSegment.call(this, stage);
};
CanvasArcToSegmentFill.prototype.draw = function(context) {
    context.fillStyle = this._color;
    context.beginPath();
    context.moveTo(this._point1.x, this._point1.y);
    context.arcTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y, this._radius);
    context.fill();
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

function CanvasArcSegmentFill(stage) {
    CanvasArcSegment.call(this, stage);
};
CanvasArcSegmentFill.prototype.draw = function(context) {
    context.fillStyle = this._color;
    context.beginPath();
    context.arc(this._point.x, this._point.y, this._radius, this._startAngle, this._endAngle, this._counterclockwise);
    context.fill();
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

// === STAGES ===

function SimpleCanvasPathStrokeStage(element, options, canvasObject) {
    SimpleCanvasStage.call(this, element, options, canvasObject);
}
SimpleCanvasPathStrokeStage.prototype = Object.create(SimpleCanvasStage.prototype);
SimpleCanvasPathStrokeStage.prototype.constructor = SimpleCanvasPathStrokeStage;
SimpleCanvasPathStrokeStage.prototype.animate = function() {
    var context = this.context;
    context.lineWidth = this.randomInt(1, 20);
    context.strokeStyle = this.randomColor();
    context.beginPath();
    context.moveTo(0,0);
    this._objects.forEach(function(object) {
        object.draw(context);
    });
    context.stroke();
}

function SimpleCanvasPathFillStage(element, options, canvasObject) {
    SimpleCanvasStage.call(this, element, options, canvasObject);
}
SimpleCanvasPathFillStage.prototype = Object.create(SimpleCanvasStage.prototype);
SimpleCanvasPathFillStage.prototype.constructor = SimpleCanvasPathFillStage;
SimpleCanvasPathFillStage.prototype.animate = function() {
    var context = this.context;
    context.fillStyle = this.randomColor();
    context.beginPath();
    context.moveTo(0,0);
    this._objects.forEach(function(object) {
        object.draw(context);
    });
    context.fill();
}

function CanvasLineSegmentStage(element, options)
{
    SimpleCanvasStage.call(this, element, options, CanvasLineSegment);
    this.context.lineCap = options["lineCap"] || "butt";
}
CanvasLineSegmentStage.prototype = Object.create(SimpleCanvasStage.prototype);
CanvasLineSegmentStage.prototype.constructor = CanvasLineSegmentStage;

function CanvasLinePathStage(element, options)
{
    SimpleCanvasPathStrokeStage.call(this, element, options, CanvasLinePoint);
    this.context.lineJoin = options["lineJoin"] || "bevel";
}
CanvasLinePathStage.prototype = Object.create(SimpleCanvasPathStrokeStage.prototype);
CanvasLinePathStage.prototype.constructor = CanvasLinePathStage;

function CanvasLineDashStage(element, options)
{
    SimpleCanvasStage.call(this, element, options, CanvasLinePoint);
    this.context.setLineDash([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    this.context.lineWidth = 1;
    this.context.strokeStyle = "#000";
    this._step = 0;
}
CanvasLineDashStage.prototype = Object.create(SimpleCanvasStage.prototype);
CanvasLineDashStage.prototype.constructor = CanvasLineDashStage;
CanvasLineDashStage.prototype.animate = function() {
    var context = this.context;
    context.lineDashOffset = this._step++;
    context.beginPath();
    context.moveTo(0,0);
    this._objects.forEach(function(object) {
        object.draw(context);
    });
    context.stroke();
};

// === BENCHMARK ===

function CanvasPathBenchmark(suite, test, options, progressBar) {
    SimpleCanvasBenchmark.call(this, suite, test, options, progressBar);
}
CanvasPathBenchmark.prototype = Object.create(SimpleCanvasBenchmark.prototype);
CanvasPathBenchmark.prototype.constructor = CanvasPathBenchmark;
CanvasPathBenchmark.prototype.createStage = function(element)
{
    switch (this._options["pathType"]) {
    case "line":
        return new CanvasLineSegmentStage(element, this._options);
    case "linePath": {
        if ("lineJoin" in this._options)
            return new CanvasLinePathStage(element, this._options);
        if ("lineDash" in this._options)
            return new CanvasLineDashStage(element, this._options);
        break;
    }
    case "quadratic":
        return new SimpleCanvasStage(element, this._options, CanvasQuadraticSegment);
    case "quadraticPath":
        return new SimpleCanvasPathStrokeStage(element, this._options, CanvasQuadraticPoint);
    case "bezier":
        return new SimpleCanvasStage(element, this._options, CanvasBezierSegment);
    case "bezierPath":
        return new SimpleCanvasPathStrokeStage(element, this._options, CanvasBezierPoint);
    case "arcTo":
        return new SimpleCanvasStage(element, this._options, CanvasArcToSegment);
    case "arc":
        return new SimpleCanvasStage(element, this._options, CanvasArcSegment);
    case "rect":
        return new SimpleCanvasStage(element, this._options, CanvasRect);
    case "lineFill":
        return new SimpleCanvasPathFillStage(element, this._options, CanvasLinePoint);
    case "quadraticFill":
        return new SimpleCanvasPathFillStage(element, this._options, CanvasQuadraticPoint);
    case "bezierFill":
        return new SimpleCanvasPathFillStage(element, this._options, CanvasBezierPoint);
    case "arcToFill":
        return new SimpleCanvasStage(element, this._options, CanvasArcToSegmentFill);
    case "arcFill":
        return new SimpleCanvasStage(element, this._options, CanvasArcSegmentFill);
    case "rectFill":
        return new SimpleCanvasStage(element, this._options, CanvasRectFill);
    }
}

window.benchmarkClient.create = function(suite, test, options, progressBar) {
    return new CanvasPathBenchmark(suite, test, options, progressBar);
}
