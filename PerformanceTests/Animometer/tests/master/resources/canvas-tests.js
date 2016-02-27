(function() {

// === PAINT OBJECTS ===

CanvasLineSegment = Utilities.createClass(
    function(stage)
    {
        var circle = Stage.randomInt(0, 2);
        this._color = ["#e01040", "#10c030", "#e05010"][circle];
        this._lineWidth = Math.pow(Pseudo.random(), 12) * 20 + 3;
        this._omega = Pseudo.random() * 3 + 0.2;
        var theta = Stage.randomAngle();
        this._cosTheta = Math.cos(theta);
        this._sinTheta = Math.sin(theta);
        this._startX = stage.circleRadius * this._cosTheta + stage.circleX[circle];
        this._startY = stage.circleRadius * this._sinTheta + stage.circleY[circle];
        this._length = Math.pow(Pseudo.random(), 8) * stage.lineLengthMaximum + stage.lineMinimum;
        this._segmentDirection = Pseudo.random() > 0.5 ? -1 : 1;
    }, {

    draw: function(context)
    {
        context.strokeStyle = this._color;
        context.lineWidth = this._lineWidth;

        this._length += Math.sin(Stage.dateCounterValue(100) * this._omega);

        context.beginPath();
        context.moveTo(this._startX, this._startY);
        context.lineTo(this._startX + this._segmentDirection * this._length * this._cosTheta,
                       this._startY + this._segmentDirection * this._length * this._sinTheta);
        context.stroke();
    }
});

CanvasArc = Utilities.createClass(
    function(stage)
    {
        var maxX = 6, maxY = 3;
        var distanceX = stage.size.x / maxX;
        var distanceY = stage.size.y / (maxY + 1);
        var randY = Stage.randomInt(0, maxY);
        var randX = Stage.randomInt(0, maxX - 1 * (randY % 2));

        this._point = new Point(distanceX * (randX + (randY % 2) / 2), distanceY * (randY + .5));

        this._radius = 20 + Math.pow(Pseudo.random(), 5) * (Math.min(distanceX, distanceY) / 1.8);
        this._startAngle = Stage.randomAngle();
        this._endAngle = Stage.randomAngle();
        this._omega = (Pseudo.random() - 0.5) * 0.3;
        this._counterclockwise = Stage.randomBool();
        var colors = ["#101010", "#808080", "#c0c0c0"];
        colors.push(["#e01040", "#10c030", "#e05010"][(randX + Math.ceil(randY / 2)) % 3]);
        this._color = colors[Math.floor(Pseudo.random() * colors.length)];
        this._lineWidth = 1 + Math.pow(Pseudo.random(), 5) * 30;
        this._doStroke = Stage.randomInt(0, 3) != 0;
    }, {

    draw: function(context)
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
    }
});

// CanvasLinePoint contains no draw() method since it is either moveTo or
// lineTo depending on its index.
CanvasLinePoint = Utilities.createClass(
    function(stage, coordinateMaximum)
    {
        var X_LOOPS = 40;
        var Y_LOOPS = 20;

        var offsets = [[-2, -1], [2, 1], [-1, 0], [1, 0], [-1, 2], [1, -2]];
        var offset = offsets[Math.floor(Pseudo.random() * offsets.length)];

        this.coordinate = new Point(X_LOOPS/2, Y_LOOPS/2);
        if (stage.objects.length) {
            var head = stage.objects[stage.objects.length - 1].coordinate;
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
        var colors = ["#101010", "#808080", "#c0c0c0", "#101010", "#808080", "#c0c0c0", "#e01040"];
        this.color = colors[Math.floor(Pseudo.random() * colors.length)];

        this.width = Math.pow(Pseudo.random(), 5) * 20 + 1;
        this.isSplit = Pseudo.random() > 0.9;
        this.point = new Point(randX, randY);
    }
);

// === STAGES ===

CanvasLineSegmentStage = Utilities.createSubclass(SimpleCanvasStage,
    function()
    {
        SimpleCanvasStage.call(this, CanvasLineSegment);
    }, {

    initialize: function(benchmark, options)
    {
        SimpleCanvasStage.prototype.initialize.call(this, benchmark, options);
        this.context.lineCap = options["lineCap"] || "butt";
        this.lineMinimum = 20;
        this.lineLengthMaximum = 40;
        this.circleRadius = this.size.x / 3 / 2 - .8 * (this.lineMinimum + this.lineLengthMaximum);
        this.circleX = [
            .55 / 3 * this.size.x,
            1.5 / 3 * this.size.x,
            2.45 / 3 * this.size.x
        ];
        this.circleY = [
            .6 * this.size.y,
            .4 * this.size.y,
            .6 * this.size.y
        ];
    },

    animate: function()
    {
        var context = this.context;

        context.clearRect(0, 0, this.size.x, this.size.y);
        context.lineWidth = 30;
        for(var i = 0; i < 3; i++) {
            context.strokeStyle = ["#e01040", "#10c030", "#e05010"][i];
            context.fillStyle = ["#70051d", "#016112", "#702701"][i];
            context.beginPath();
                context.arc(this.circleX[i], this.circleY[i], this.circleRadius, 0, Math.PI*2);
            context.stroke();
            context.fill();
        }

        for (var i = this.offsetIndex, length = this.objects.length; i < length; ++i)
            this.objects[i].draw(context);
    }
});

CanvasLinePathStage = Utilities.createSubclass(SimpleCanvasStage,
    function()
    {
        SimpleCanvasStage.call(this, CanvasLinePoint);
    }, {

    initialize: function(benchmark, options)
    {
        SimpleCanvasStage.prototype.initialize.call(this, benchmark, options);
        this.context.lineJoin = options["lineJoin"] || "bevel";
        this.context.lineCap = options["lineCap"] || "butt";
    },

    animate: function() {
        var context = this.context;

        context.clearRect(0, 0, this.size.x, this.size.y);
        context.beginPath();
        for (var i = this.offsetIndex, length = this.objects.length; i < length; ++i) {
            var object = this.objects[i];
            if (i == this.offsetIndex) {
                context.lineWidth = object.width;
                context.strokeStyle = object.color;
                context.moveTo(object.point.x, object.point.y);
            } else {
                if (object.isSplit) {
                    context.stroke();

                    context.lineWidth = object.width;
                    context.strokeStyle = object.color;
                    context.beginPath();
                }

                context.lineTo(object.point.x, object.point.y);

                if (Pseudo.random() > 0.999)
                    object.isSplit = !object.isSplit;
            }
        }
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
        case "linePath":
            stage = new CanvasLinePathStage();
            break;
        case "arcs":
            stage = new SimpleCanvasStage(CanvasArc);
            break;
        }

        Benchmark.call(this, stage, options);
    }
);

window.benchmarkClass = CanvasPathBenchmark;

})();