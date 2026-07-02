//
// To use this script, the HTML has to have only one element and this element's className
// should be equal to "Benchmark".
//
// This script forces rendering the HTML element many times by changing its position and its
// size. The size can be very big to test the case of displaying only a small portion of the
// element and measure how fast the display will be in this case.
//

function Point(x, y) {
    this.x = x;
    this.y = y;
}

Point.prototype = {
    scale : function(other) {
        return new Point(this.x * other.x, this.y * other.y);
    },
    add : function(other) {
        return new Point(this.x + other.x, this.y + other.y);
    },
    subtract : function(other) {
        return new Point(this.x - other.x, this.y - other.y);
    },
    isAlmostEqual : function(other, threshold) {
        return Math.abs(this.x - other.x) < threshold &&  Math.abs(this.y - other.y) < threshold;
    }
}

function Rectangle(position, size) {
    this.position = position;
    this.size = size;
}

Rectangle.prototype = {
    left : function() {
        return this.position.x;
    },
    top : function() {
        return this.position.y;
    },
    width : function() {
        return this.size.x;
    },
    height : function() {
        return this.size.y;
    },
    isAlmostEqual : function(other, threshold) {
        return this.position.isAlmostEqual(other.position, threshold) && this.size.isAlmostEqual(other.size, threshold);
    }
}

function ElapsedTime() {
    this._startTime = PerfTestRunner.now();
    this._stopTime = this._startTime;
}

ElapsedTime.prototype = {
    
    start : function() {
        this._startTime = this._stopTime = PerfTestRunner.now();
    },

    stop : function() {
        this._stopTime = PerfTestRunner.now();
    },

    isActive : function() {
        return this._startTime == this._stopTime;
    },

    elapsed : function() {
        return (this.isActive() ? PerfTestRunner.now() : this._stopTime) - this._startTime;
    },

    elapsedString : function() {
        var tenths = this.elapsed() / 1000;
        return tenths.toFixed(2) + " Seconds";
    }
}

function AnimateMove(offset, zoomFactor, animateFactor) {
    this.offset = offset;
    this.zoomFactor = zoomFactor;
    this.animateFactor = animateFactor;
}

AnimateMove.centerFactor = new Point(0.5, 0.5);

AnimateMove.prototype = {
    
    targetRect : function(windowSize, sourceSize) {
        var offset = this.offset.scale(this.zoomFactor);
        var size = sourceSize.scale(this.zoomFactor);
        var position = windowSize.subtract(size).scale(AnimateMove.centerFactor).subtract(offset);
        return new Rectangle(position, size);
    },

    nextAnimateRect : function(targetRect, animateRect) {
        var deltaPosition = targetRect.position.subtract(animateRect.position).scale(this.animateFactor);
        var deltaSize = targetRect.size.subtract(animateRect.size).scale(this.animateFactor);
        return new Rectangle(animateRect.position.add(deltaPosition), animateRect.size.add(deltaSize));
    }
}

function ElementAnimator(element, windowSize, sourceSize) {
    this._element = element;
    this._windowSize = windowSize;
    this._sourceSize = sourceSize;

    this._animateMoves = [
        new AnimateMove(new Point(   0,    0), new Point( 0.7,  0.7), new Point(1.00, 1.00)),
        new AnimateMove(new Point(-500, -300), new Point(12.0, 12.0), new Point(0.50, 0.50)),
        new AnimateMove(new Point( 100, -200), new Point( 0.1,  0.1), new Point(0.50, 0.50)),
        new AnimateMove(new Point(-100, -300), new Point( 5.0,  5.0), new Point(0.20, 0.20)),
        new AnimateMove(new Point(   0,    0), new Point( 0.7,  0.7), new Point(0.50, 0.50))
    ];

    this._animateMoveIndex = 0;
    this.nextTargetRect();
    this._animateRect = this._targetRect;
    this.moveToAnimateRect();
}

ElementAnimator.prototype = {
    
    nextTargetRect : function() {
        if (this._animateMoveIndex >= this._animateMoves.length)
            return false;

        this._targetRect = this._animateMoves[this._animateMoveIndex++].targetRect(this._windowSize, this._sourceSize);
        return true;
    },
    
    nextAnimateRect : function() {
        if (this._animateRect.isAlmostEqual(this._targetRect, 0.1))
            return false;

        this._animateRect = this._animateMoves[this._animateMoveIndex - 1].nextAnimateRect(this._targetRect, this._animateRect);
        return true;
    },
    
    moveToAnimateRect : function() {
        this._element.style.width = this._animateRect.width() + "px";
        this._element.style.left = this._animateRect.left() + "px";
        this._element.style.top = this._animateRect.top() + "px";
    }
}

function RenderAnimator() {
    this.element = document.getElementsByClassName("Benchmark")[0];
    this.sourceSize = new Point(this.element.width, this.element.height);
    
    // Tiling causes the rendering to slow down when the window width > 2000
    this.windowSize = new Point(3000, 1500);
    
    this.timer = document.createElement("span");
    this.timer.className = "Timer";
    document.body.appendChild(this.timer);
    
    this.timeoutDelay = window.testRunner ? 0 : 500;
    this.elapsedTime = new ElapsedTime();
    this.elementAnimator = null;
}

RenderAnimator.prototype = {    

    nextRun : function() {
        this.showElements(true);
        this.elementAnimator = new ElementAnimator(this.element, this.windowSize, this.sourceSize);
        
        var self = this;
        setTimeout(function () {
            self.elapsedTime.start();
            self.moveToNextTargetRect();
        }, this.timeoutDelay);
    },

    moveToNextTargetRect : function() {
        if (this.elementAnimator.nextTargetRect())
            setTimeout(this.moveToNextAnimateRect.bind(this), 0);
        else {
            this.elapsedTime.stop();
            setTimeout(this.finishRun.bind(this), this.timeoutDelay);
        }
    },
    
    moveToNextAnimateRect : function() {
        this.timer.innerHTML = this.elapsedTime.elapsedString();
        
        if (this.elementAnimator.nextAnimateRect())
            window.requestAnimationFrame(this.moveToNextAnimateRect.bind(this));
        else
            this.moveToNextTargetRect();

        this.elementAnimator.moveToAnimateRect();
    },
    
    finishRun : function() {
        this.showElements(false);
        
        var finishedTest = !PerfTestRunner.measureValueAsync(this.elapsedTime.elapsed());
        PerfTestRunner.gc();
        
        if (!finishedTest)
            setTimeout(this.nextRun.bind(this), this.timeoutDelay * 2);
    },

    showElements : function(show) {
        this.element.style.visibility = "visible";
        this.element.style.opacity = show ? 1 : 0;
        this.timer.style.opacity = show ? 1 : 0;
    },
    
    removeElements : function() {
        this.element.parentNode.removeChild(this.element);
        this.timer.parentNode.removeChild(this.timer);
        this.element = null;
        this.timer = null;
    }
}

window.addEventListener("load", function() {
    window.renderAnimator = new RenderAnimator();
    window.renderAnimator.nextRun();
});

PerfTestRunner.prepareToMeasureValuesAsync({
    unit: 'ms',
    done: function () {
        window.renderAnimator.removeElements();
    }
});
