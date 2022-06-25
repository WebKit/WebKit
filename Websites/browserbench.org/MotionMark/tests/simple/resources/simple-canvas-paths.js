(function() {

// === PAINT OBJECTS ===

CanvasLineSegment = Utilities.createClass(
    function(stage) {
        var radius = Stage.randomInt(10, 100);
        var center = Stage.randomPosition(stage.size);
        var delta = Point.pointOnCircle(Stage.randomAngle(), radius/2);

        this._point1 = center.add(delta);
        this._point2 = center.subtract(delta);
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 100);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.moveTo(this._point1.x, this._point1.y);
        context.lineTo(this._point2.x, this._point2.y);
        context.stroke();
    }
});

CanvasLinePoint = Utilities.createClass(
    function(stage, coordinateMaximumFactor) {
        var pointMaximum = new Point(Math.min(stage.size.x, coordinateMaximumFactor * stage.size.x), Math.min(stage.size.y, coordinateMaximumFactor * stage.size.y));
        this._point = Stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
    }, {

    draw: function(context) {
        context.lineTo(this._point.x, this._point.y);
    }
})

CanvasQuadraticSegment = Utilities.createClass(
    function(stage) {
        var maxSize = Stage.randomInt(20, 200);
        var toCenter = Stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

        this._point1 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point2 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point3 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 50);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.moveTo(this._point1.x, this._point1.y);
        context.quadraticCurveTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y);
        context.stroke();
    }
});

CanvasQuadraticPoint = Utilities.createClass(
    function(stage, coordinateMaximumFactor) {
        var pointMaximum = Stage.randomPosition(new Point(Math.min(stage.size.x, coordinateMaximumFactor * stage.size.x), Math.min(stage.size.y, coordinateMaximumFactor * stage.size.y)));
        this._point1 = Stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
        this._point2 = Stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
    }, {

    draw: function(context) {
        context.quadraticCurveTo(this._point1.x, this._point1.y, this._point2.x, this._point2.y);
    }
});

CanvasBezierSegment = Utilities.createClass(
    function(stage) {
        var maxSize = Stage.randomInt(20, 200);
        var toCenter = Stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

        this._point1 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point2 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point3 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point4 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 50);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.moveTo(this._point1.x, this._point1.y);
        context.bezierCurveTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y, this._point4.x, this._point4.y);
        context.stroke();
    }
});

CanvasBezierPoint = Utilities.createClass(
    function(stage, coordinateMaximumFactor) {
        var pointMaximum = Stage.randomPosition(new Point(Math.min(stage.size.x, coordinateMaximumFactor * stage.size.x), Math.min(stage.size.y, coordinateMaximumFactor * stage.size.y)));
        this._point1 = Stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
        this._point2 = Stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
        this._point3 = Stage.randomPosition(pointMaximum).add(new Point((stage.size.x - pointMaximum.x) / 2, (stage.size.y - pointMaximum.y) / 2));
    }, {

    draw: function(context) {
        context.bezierCurveTo(this._point1.x, this._point1.y, this._point2.x, this._point2.y, this._point3.x, this._point3.y);
    }
});

CanvasArcToSegment = Utilities.createClass(
    function(stage) {
        var maxSize = Stage.randomInt(20, 200);
        var toCenter = Stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

        this._point1 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point2 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._point3 = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._radius = Stage.randomInt(20, 200);
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 50);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.moveTo(this._point1.x, this._point1.y);
        context.arcTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y, this._radius);
        context.stroke();
    }
});

CanvasArcToSegmentFill = Utilities.createClass(
    function(stage) {
        CanvasArcToSegment.call(this, stage);
    }, {

    draw: function(context) {
        context.fillStyle = this._color;
        context.beginPath();
        context.moveTo(this._point1.x, this._point1.y);
        context.arcTo(this._point2.x, this._point2.y, this._point3.x, this._point3.y, this._radius);
        context.fill();
    }
});

CanvasArcSegment = Utilities.createClass(
    function(stage) {
        var maxSize = Stage.randomInt(20, 200);
        var toCenter = Stage.randomPosition(stage.size).subtract(new Point(maxSize/2, maxSize/2));

        this._point = Stage.randomPosition(new Point(maxSize, maxSize)).add(toCenter);
        this._radius = Stage.randomInt(20, 200);
        this._startAngle = Stage.randomAngle();
        this._endAngle = Stage.randomAngle();
        this._counterclockwise = Stage.randomBool();
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 50);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.arc(this._point.x, this._point.y, this._radius, this._startAngle, this._endAngle, this._counterclockwise);
        context.stroke();
    }
});

CanvasArcSegmentFill = Utilities.createClass(
    function(stage) {
        CanvasArcSegment.call(this, stage);
    }, {

    draw: function(context) {
        context.fillStyle = this._color;
        context.beginPath();
        context.arc(this._point.x, this._point.y, this._radius, this._startAngle, this._endAngle, this._counterclockwise);
        context.fill();
    }
});

CanvasRect = Utilities.createClass(
    function(stage) {
        this._width = Stage.randomInt(20, 200);
        this._height = Stage.randomInt(20, 200);
        this._point = Stage.randomPosition(stage.size).subtract(new Point(this._width/2, this._height/2));
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 20);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.rect(this._point.x, this._point.y, this._width, this._height);
        context.stroke();
    }
});

CanvasRectFill = Utilities.createClass(
    function(stage) {
        CanvasRect.call(this, stage);
    }, {

    draw: function(context) {
        context.fillStyle = this._color;
        context.beginPath();
        context.rect(this._point.x, this._point.y, this._width, this._height);
        context.fill();
    }
});

CanvasEllipse = Utilities.createClass(
    function(stage) {
        this._radius = new Point(Stage.randomInt(20, 200), Stage.randomInt(20, 200));
        var toCenter = Stage.randomPosition(stage.size).subtract(this._radius.multiply(.5));

        this._center = Stage.randomPosition(this._radius).add(toCenter);
        this._rotation = Stage.randomAngle();
        this._startAngle = Stage.randomAngle();
        this._endAngle = Stage.randomAngle();
        this._anticlockwise = Stage.randomBool();
        this._color = Stage.randomColor();
        this._lineWidth = Stage.randomInt(1, 20);
    }, {

    draw: function(context) {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;
        context.beginPath();
        context.ellipse(this._center.x, this._center.y, this._radius.width, this._radius.height, this._rotation, this._startAngle, this._endAngle, this._anticlockwise);
        context.stroke();
    }
});

CanvasEllipseFill = Utilities.createClass(
    function(stage) {
        CanvasEllipse.call(this, stage);
    }, {

    draw: function(context) {
        context.fillStyle = this._color;
        context.beginPath();
        context.ellipse(this._center.x, this._center.y, this._radius.width, this._radius.height, this._rotation, this._startAngle, this._endAngle, this._anticlockwise);
        context.fill();
    }
});

CanvasStroke = Utilities.createClass(
    function (stage) {
        this._object = new (Stage.randomElementInArray(this.objectTypes))(stage);
    }, {

    objectTypes: [
        CanvasQuadraticSegment,
        CanvasBezierSegment,
        CanvasArcToSegment,
        CanvasArcSegment,
        CanvasRect,
        CanvasEllipse
    ],

    draw: function(context) {
        this._object.draw(context);
    }
});

CanvasFill = Utilities.createClass(
    function (stage) {
        this._object = new (Stage.randomElementInArray(this.objectTypes))(stage);
    }, {

    objectTypes: [
        CanvasArcToSegmentFill,
        CanvasArcSegmentFill,
        CanvasRectFill,
        CanvasEllipseFill
    ],

    draw: function(context) {
        this._object.draw(context);
    }
});

// === STAGES ===

SimpleCanvasPathStrokeStage = Utilities.createSubclass(SimpleCanvasStage,
    function(canvasObject) {
        SimpleCanvasStage.call(this, canvasObject);
    }, {

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        context.lineWidth = Stage.randomInt(1, 20);
        context.strokeStyle = Stage.rotatingColor();
        context.beginPath();
        context.moveTo(this.size.x / 2, this.size.y / 2);
        for (var i = 0, length = this.offsetIndex; i < length; ++i)
            this.objects[i].draw(context);
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
        context.fillStyle = Stage.rotatingColor();
        context.beginPath();
        context.moveTo(this.size.x / 2, this.size.y / 2);
        for (var i = 0, length = this.offsetIndex; i < length; ++i)
            this.objects[i].draw(context);
        context.fill();
    }
});

CanvasLineSegmentStage = Utilities.createSubclass(SimpleCanvasStage,
    function()
    {
        SimpleCanvasStage.call(this, CanvasLineSegment);
    }, {

    initialize: function(benchmark, options)
    {
        SimpleCanvasStage.prototype.initialize.call(this, benchmark, options);
        this.context.lineCap = options["lineCap"] || "butt";
    }
});

CanvasLinePathStage = Utilities.createSubclass(SimpleCanvasPathStrokeStage,
    function()
    {
        SimpleCanvasPathStrokeStage.call(this, CanvasLinePoint);
    }, {

    initialize: function(benchmark, options)
    {
        SimpleCanvasPathStrokeStage.prototype.initialize.call(this, benchmark, options);
        this.context.lineJoin = options["lineJoin"] || "bevel";
    }
});

CanvasLineDashStage = Utilities.createSubclass(SimpleCanvasStage,
    function()
    {
        SimpleCanvasStage.call(this, CanvasLinePoint);
        this._step = 0;
    }, {

    initialize: function(benchmark, options)
    {
        SimpleCanvasStage.prototype.initialize.call(this, benchmark, options);
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
        for (var i = 0, length = this.offsetIndex; i < length; ++i)
            this.objects[i].draw(context);
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
        case "ellipse":
            stage = new SimpleCanvasStage(CanvasEllipse);
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
        case "ellipseFill":
            stage = new SimpleCanvasStage(CanvasEllipseFill);
            break;
        case "strokes":
            stage = new SimpleCanvasStage(CanvasStroke);
            break;
        case "fills":
            stage = new SimpleCanvasStage(CanvasFill);
            break;
        }

        Benchmark.call(this, stage, options);
    }
);

window.benchmarkClass = CanvasPathBenchmark;

})();