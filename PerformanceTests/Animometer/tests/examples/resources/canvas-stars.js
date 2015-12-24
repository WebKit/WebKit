function CanvasStar(stage)
{
    this._context = stage.context;
    this._stageSize = stage.size;

    this._size = stage.randomSquareSize(5, 20);
    this._center = stage.randomPosition(stage.size.subtract(this._size)).add(this._size.center);
    this._rotateX = 0;
    this._rotateDeltaX = stage.random(0.3, 0.7);
}

CanvasStar.prototype._draw = function()
{
    this._context.save();
    this._context.translate(this._center.x, this._center.y);
    
    this._context.fillStyle = 'yellow';
    this._context.strokeStyle = 'white';

    this._context.lineWidth = 1;
    
    this._context.beginPath();
    this._context.moveTo(                                     0, -this._size.y /  2);
    this._context.lineTo(+this._size.x / 20 - this._rotateX / 5, -this._size.y / 10);
    this._context.lineTo(+this._size.x / 4  - this._rotateX,                      0);
    this._context.lineTo(+this._size.x / 20 - this._rotateX / 5, +this._size.y / 10);
    this._context.lineTo(                                     0, +this._size.y /  2);
    this._context.lineTo(-this._size.x / 20 + this._rotateX / 5, +this._size.y / 10);
    this._context.lineTo(-this._size.x /  4 + this._rotateX,                      0);
    this._context.lineTo(-this._size.x / 20 + this._rotateX / 5, -this._size.y / 10);
    this._context.lineTo(                                     0, -this._size.y /  2);
    
    this._context.fill();
    this._context.stroke();
    this._context.closePath();
    this._context.restore();
}

CanvasStar.prototype.animate = function(timeDelta)
{
    this._rotateX += this._rotateDeltaX;
    
    if (this._rotateX > this._size.x / 4 || this._rotateX < -this._size.x / 4) {
        this._rotateDeltaX = -this._rotateDeltaX;
        this._rotateX += this._rotateDeltaX;
    }

   this._draw();
}

function CanvasStarsStage(element, options)
{
   Stage.call(this, element, options);
   this.context = this.element.getContext("2d");

    this._objects = [];
}

CanvasStarsStage.prototype = Object.create(Stage.prototype);
CanvasStarsStage.prototype.constructor = CanvasStarsStage;

CanvasStarsStage.prototype.tune = function(count)
{
    if (count == 0)
        return this._objects.length;

    if (count > 0) {
        for (var i = 0; i < count; ++i)
            this._objects.push(new CanvasStar(this));
        return this._objects.length;
    }

    count = Math.min(-count, this._objects.length);
    this._objects.splice(-count, count);

    return this._objects.length;
}

CanvasStarsStage.prototype.animate = function(timeDelta)
{
    this._objects.forEach(function(object) {
        object.animate(timeDelta);
    });
}

function CanvasStarsAnimator(benchmark, options)
{
   Animator.call(this, benchmark, options);
   this._context = benchmark._stage.context;
}

CanvasStarsAnimator.prototype = Object.create(StageAnimator.prototype);
CanvasStarsAnimator.prototype.constructor = CanvasStarsAnimator;

CanvasStarsAnimator.prototype.animate = function()
{
    this._context.beginPath();
    this._context.fillStyle = 'black';
    this._context.rect(0, 0, this._benchmark._stage.size.width, this._benchmark._stage.size.height);
    this._context.fill();
   return StageAnimator.prototype.animate.call(this);
}

function CanvasStarsBenchmark(suite, test, options, progressBar)
{
   StageBenchmark.call(this, suite, test, options, progressBar);
}

CanvasStarsBenchmark.prototype = Object.create(StageBenchmark.prototype);
CanvasStarsBenchmark.prototype.constructor = CanvasStarsBenchmark;

CanvasStarsBenchmark.prototype.createStage = function(element)
{
   // Attach the stage to the benchmark.
   return new CanvasStarsStage(element, this._options);
}

CanvasStarsBenchmark.prototype.createAnimator = function()
{
   // Attach the animator to the benchmark.
   return new CanvasStarsAnimator(this, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
   // This function is called from the test harness which starts the
   // test by creating your benchmark object.
   return new CanvasStarsBenchmark(suite, test, options, progressBar);
}