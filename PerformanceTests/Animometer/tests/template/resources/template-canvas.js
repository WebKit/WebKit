function TemplateCanvasObject(stage)
{
    // For the canvas stage, most likely you will need to create your
    // animated object since it's only draw time thing.
    
    // Fill in your object data.
}

TemplateCanvasObject.prototype._draw = function()
{
    // Draw your object.
}

TemplateCanvasObject.prototype.animate = function(timeDelta)
{
    // Redraw the animated object. The last time this animated
    // item was drawn before 'timeDelta'.
    
    // Move your object.
    
    // Redraw your object.
    this._draw();
}

function TemplateCanvasStage(element, options)
{
    Stage.call(this, element, options);
    this.context = this.element.getContext("2d");

    // Define a collection for your objects.
}

TemplateCanvasStage.prototype = Object.create(Stage.prototype);
TemplateCanvasStage.prototype.constructor = TemplateCanvasStage;

TemplateCanvasStage.prototype.tune = function(count)
{
    // If count is -ve, -count elements need to be removed form the
    // stage. If count is +ve, +count elements need to be added to
    // the stage.
    
    // Change objects in the stage.
    
    // Return the number of all the elements in the stage.
    // This number is recorded in the sampled data.
    
    // Return the count of the objects in the stage.
    return 0;
}

TemplateCanvasStage.prototype.animate = function(timeDelta)
{
    // Animate the elements such that all of them are redrawn. Most
    // likely you will need to call TemplateCanvasObject.animate()
    // for all your animated objects here.

    // Loop through all your objects and ask them to animate.
}

function TemplateCanvasAnimator(benchmark, options)
{
    Animator.call(this, benchmark, options);
    this._context = benchmark._stage.context;
}

TemplateCanvasAnimator.prototype = Object.create(StageAnimator.prototype);
TemplateCanvasAnimator.prototype.constructor = TemplateCanvasAnimator;

TemplateCanvasAnimator.prototype.animate = function()
{
    // Most likely you will need to clear the canvas with every redraw.
    this._context.clearRect(0, 0, this._benchmark._stage.size.x, this._benchmark._stage.size.y);

    // Draw scene stuff here if needed.

    return StageAnimator.prototype.animate.call(this);
}

function TemplateCanvasBenchmark(suite, test, options, progressBar)
{
    StageBenchmark.call(this, suite, test, options, progressBar);
}

TemplateCanvasBenchmark.prototype = Object.create(StageBenchmark.prototype);
TemplateCanvasBenchmark.prototype.constructor = TemplateCanvasBenchmark;

TemplateCanvasBenchmark.prototype.createStage = function(element)
{
    // Attach the stage to the benchmark.
    return new TemplateCanvasStage(element, this._options);
}

TemplateCanvasBenchmark.prototype.createAnimator = function()
{
    // Attach the animator to the benchmark.
    return new TemplateCanvasAnimator(this, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    // This function is called from the test harness which starts the
    // test by creating your benchmark object.
    return new TemplateCanvasBenchmark(suite, test, options, progressBar);
}
