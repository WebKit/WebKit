(function() {

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

function CanvasLinePoint(stage, coordinateMaximumFactor) {
    var pointMaximum = new Point(Math.min(stage.size.x, coordinateMaximumFactor * stage.size.x), Math.min(stage.size.y, coordinateMaximumFactor * stage.size.y));
    this._point = stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
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

function CanvasQuadraticPoint(stage, coordinateMaximumFactor) {
    var pointMaximum = stage.randomPosition(new Point(Math.min(stage.size.x, coordinateMaximumFactor * stage.size.x), Math.min(stage.size.y, coordinateMaximumFactor * stage.size.y)));
    this._point1 = stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
    this._point2 = stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
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

function CanvasBezierPoint(stage, coordinateMaximumFactor) {
    var pointMaximum = stage.randomPosition(new Point(Math.min(stage.size.x, coordinateMaximumFactor * stage.size.x), Math.min(stage.size.y, coordinateMaximumFactor * stage.size.y)));
    this._point1 = stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
    this._point2 = stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
    this._point3 = stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
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

SimpleCanvasPathStrokeStage = Utilities.createSubclass(SimpleCanvasStage,
    function(canvasObject) {
        SimpleCanvasStage.call(this, canvasObject);
    }, {

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        context.lineWidth = this.randomInt(1, 20);
        context.strokeStyle = this.randomColor();
        context.beginPath();
        context.moveTo(this.size.x / 2, this.size.y / 2);
        this.objects.forEach(function(object) {
            object.draw(context);
        });
        context.stroke();
    }
});

SimpleCanvasPathFillStage = Utilities.createSubclass(SimpleCanvasStage,
    function(canvasObject) {
        SimpleCanvasStage.call(this, canvasObject);
    }, {

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        context.fillStyle = this.randomColor();
        context.beginPath();
        context.moveTo(this.size.x / 2, this.size.y / 2);
        this.objects.forEach(function(object) {
            object.draw(context);
        });
        context.fill();
    }
});

CanvasLineSegmentStage = Utilities.createSubclass(SimpleCanvasStage,
    function()
    {
        SimpleCanvasStage.call(this, CanvasLineSegment);
    }, {

    initialize: function(benchmark)
    {
        SimpleCanvasStage.prototype.initialize.call(this, benchmark);
        this.context.lineCap = benchmark.options["lineCap"] || "butt";
    }
});

CanvasLinePathStage = Utilities.createSubclass(SimpleCanvasPathStrokeStage,
    function()
    {
        SimpleCanvasPathStrokeStage.call(this, CanvasLinePoint);
    }, {

    initialize: function(benchmark)
    {
        SimpleCanvasPathStrokeStage.prototype.initialize.call(this, benchmark);
        this.context.lineJoin = benchmark.options["lineJoin"] || "bevel";
    }
});

CanvasLineDashStage = Utilities.createSubclass(SimpleCanvasStage,
    function()
    {
        SimpleCanvasStage.call(this, CanvasLinePoint);
        this._step = 0;
    }, {

    initialize: function(benchmark)
    {
        SimpleCanvasStage.prototype.initialize.call(this, benchmark);
        this.context.setLineDash([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        this.context.lineWidth = 1;
        this.context.strokeStyle = "#000";
    },

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        context.lineDashOffset = this._step++;
        context.beginPath();
        context.moveTo(this.size.x / 2, this.size.y / 2);
        this.objects.forEach(function(object) {
            object.draw(context);
        });
        context.stroke();
    }
});

// === BENCHMARK ===

CanvasPathBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        var stage;
        switch (options["pathType"]) {
        case "line":
            stage = new CanvasLineSegmentStage();
            break;
        case "linePath": {
            if ("lineJoin" in options)
                stage = new CanvasLinePathStage();
            if ("lineDash" in options)
                stage = new CanvasLineDashStage();
            break;
        }
        case "quadratic":
            stage = new SimpleCanvasStage(CanvasQuadraticSegment);
            break;
        case "quadraticPath":
            stage = new SimpleCanvasPathStrokeStage(CanvasQuadraticPoint);
            break;
        case "bezier":
            stage = new SimpleCanvasStage(CanvasBezierSegment);
            break;
        case "bezierPath":
            stage = new SimpleCanvasPathStrokeStage(CanvasBezierPoint);
            break;
        case "arcTo":
            stage = new SimpleCanvasStage(CanvasArcToSegment);
            break;
        case "arc":
            stage = new SimpleCanvasStage(CanvasArcSegment);
            break;
        case "rect":
            stage = new SimpleCanvasStage(CanvasRect);
            break;
        case "lineFill":
            stage = new SimpleCanvasPathFillStage(CanvasLinePoint);
            break;
        case "quadraticFill":
            stage = new SimpleCanvasPathFillStage(CanvasQuadraticPoint);
            break;
        case "bezierFill":
            stage = new SimpleCanvasPathFillStage(CanvasBezierPoint);
            break;
        case "arcToFill":
            stage = new SimpleCanvasStage(CanvasArcToSegmentFill);
            break;
        case "arcFill":
            stage = new SimpleCanvasStage(CanvasArcSegmentFill);
            break;
        case "rectFill":
            stage = new SimpleCanvasStage(CanvasRectFill);
            break;
        }

        Benchmark.call(this, stage, options);
    }
);

window.benchmarkClass = CanvasPathBenchmark;

})();