Utilities =
{
    _parse: function(str, sep)
    {
        var output = {};
        str.split(sep).forEach(function(part) {
            var item = part.split("=");
            var value = decodeURIComponent(item[1]);
            if (value[0] == "'" )
                output[item[0]] = value.substr(1, value.length - 2);
            else
                output[item[0]] = value;
          });
        return output;
    },

    parseParameters: function()
    {
        return this._parse(window.location.search.substr(1), "&");
    },

    parseArguments: function(str)
    {
        return this._parse(str, " ");
    },

    extendObject: function(obj1, obj2)
    {
        for (var attrname in obj2)
            obj1[attrname] = obj2[attrname];
        return obj1;
    },

    copyObject: function(obj)
    {
        return this.extendObject({}, obj);
    },

    mergeObjects: function(obj1, obj2)
    {
        return this.extendObject(this.copyObject(obj1), obj2);
    },

    createClass: function(classConstructor, classMethods)
    {
        classConstructor.prototype = classMethods;
        return classConstructor;
    },

    createSubclass: function(superclass, classConstructor, classMethods)
    {
        classConstructor.prototype = Object.create(superclass.prototype);
        classConstructor.prototype.constructor = classConstructor;
        if (classMethods)
            Utilities.extendObject(classConstructor.prototype, classMethods);
        return classConstructor;
    },

    createElement: function(name, attrs, parentElement)
    {
        var element = document.createElement(name);

        for (var key in attrs)
            element.setAttribute(key, attrs[key]);

        parentElement.appendChild(element);
        return element;
    },

    createSVGElement: function(name, attrs, xlinkAttrs, parentElement)
    {
        const svgNamespace = "http://www.w3.org/2000/svg";
        const xlinkNamespace = "http://www.w3.org/1999/xlink";

        var element = document.createElementNS(svgNamespace, name);

        for (var key in attrs)
            element.setAttribute(key, attrs[key]);

        for (var key in xlinkAttrs)
            element.setAttributeNS(xlinkNamespace, key, xlinkAttrs[key]);

        parentElement.appendChild(element);
        return element;
    },

    browserPrefix: function()
    {
        // Get the HTML element's CSSStyleDeclaration
        var styles = window.getComputedStyle(document.documentElement, '');

        // Convert the styles list to an array
        var stylesArray = Array.prototype.slice.call(styles);

        // Concatenate all the styles in one big string
        var stylesString = stylesArray.join('');

        // Search the styles string for a known prefix type, settle on Opera if none is found.
        var prefixes = stylesString.match(/-(moz|webkit|ms)-/) || (styles.OLink === '' && ['', 'o']);

        // prefixes has two elements; e.g. for webkit it has ['-webkit-', 'webkit'];
        var prefix = prefixes[1];

        // Have 'O' before 'Moz' in the string so it is matched first.
        var dom = ('WebKit|O|Moz|MS').match(new RegExp(prefix, 'i'))[0];

        // Return all the required prefixes.
        return {
            dom: dom,
            lowercase: prefix,
            css: '-' + prefix + '-',
            js: prefix[0].toUpperCase() + prefix.substr(1)
        };
    },

    setElementPrefixedProperty: function(element, property, value)
    {
        element.style[property] = element.style[this.browserPrefix().js + property[0].toUpperCase() + property.substr(1)] = value;
    },

    progressValue: function(value, min, max)
    {
        return (value - min) / (max - min);
    },

    lerp: function(value, min, max)
    {
        return min + (max - min) * value;
    }
};

Array.prototype.swap = function(i, j)
{
    var t = this[i];
    this[i] = this[j];
    this[j] = t;
    return this;
}

if (!Array.prototype.fill) {
    Array.prototype.fill = function(value) {
        if (this == null)
            throw new TypeError('Array.prototype.fill called on null or undefined');

        var object = Object(this);
        var len = parseInt(object.length, 10);
        var start = arguments[1];
        var relativeStart = parseInt(start, 10) || 0;
        var k = relativeStart < 0 ? Math.max(len + relativeStart, 0) : Math.min(relativeStart, len);
        var end = arguments[2];
        var relativeEnd = end === undefined ? len : (parseInt(end) || 0) ;
        var final = relativeEnd < 0 ? Math.max(len + relativeEnd, 0) : Math.min(relativeEnd, len);

        for (; k < final; k++)
            object[k] = value;

        return object;
    };
}

if (!Array.prototype.find) {
    Array.prototype.find = function(predicate) {
        if (this == null)
            throw new TypeError('Array.prototype.find called on null or undefined');
        if (typeof predicate !== 'function')
            throw new TypeError('predicate must be a function');

        var list = Object(this);
        var length = list.length >>> 0;
        var thisArg = arguments[1];
        var value;

        for (var i = 0; i < length; i++) {
            value = list[i];
            if (predicate.call(thisArg, value, i, list))
                return value;
        }
        return undefined;
    };
}

Array.prototype.shuffle = function()
{
    for (var index = this.length - 1; index >= 0; --index) {
        var randomIndex = Math.floor(Math.random() * (index + 1));
        this.swap(index, randomIndex);
    }
    return this;
}

Point = Utilities.createClass(
    function(x, y)
    {
        this.x = x;
        this.y = y;
    }, {

    // Used when the point object is used as a size object.
    get width()
    {
        return this.x;
    },

    // Used when the point object is used as a size object.
    get height()
    {
        return this.y;
    },

    // Used when the point object is used as a size object.
    get center()
    {
        return new Point(this.x / 2, this.y / 2);
    },

    str: function()
    {
        return "x = " + this.x + ", y = " + this.y;
    },

    add: function(other)
    {
        if(isNaN(other.x))
            return new Point(this.x + other, this.y + other);
        return new Point(this.x + other.x, this.y + other.y);
    },

    subtract: function(other)
    {
        if(isNaN(other.x))
            return new Point(this.x - other, this.y - other);
        return new Point(this.x - other.x, this.y - other.y);
    },

    multiply: function(other)
    {
        if(isNaN(other.x))
            return new Point(this.x * other, this.y * other);
        return new Point(this.x * other.x, this.y * other.y);
    },

    move: function(angle, velocity, timeDelta)
    {
        return this.add(Point.pointOnCircle(angle, velocity * (timeDelta / 1000)));
    },

    length: function() {
        return Math.sqrt( this.x * this.x + this.y * this.y );
    },

    normalize: function() {
        var l = Math.sqrt( this.x * this.x + this.y * this.y );
        this.x /= l;
        this.y /= l;
        return this;
    }
});

Utilities.extendObject(Point, {
    zero: function()
    {
        return new Point(0, 0);
    },

    pointOnCircle: function(angle, radius)
    {
        return new Point(radius * Math.cos(angle), radius * Math.sin(angle));
    },

    pointOnEllipse: function(angle, radiuses)
    {
        return new Point(radiuses.x * Math.cos(angle), radiuses.y * Math.sin(angle));
    },

    elementClientSize: function(element)
    {
        var rect = element.getBoundingClientRect();
        return new Point(rect.width, rect.height);
    }
});

Insets = Utilities.createClass(
    function(top, right, bottom, left)
    {
        this.top = top;
        this.right = right;
        this.bottom = bottom;
        this.left = left;
    }, {

    get width()
    {
        return this.left + this.right;
    },

    get height()
    {
        return this.top + this.bottom;
    },

    get size()
    {
        return new Point(this.width, this.height);
    }
});

Insets.elementPadding = function(element)
{
    var styles = window.getComputedStyle(element);
    return new Insets(
        parseFloat(styles.paddingTop),
        parseFloat(styles.paddingRight),
        parseFloat(styles.paddingBottom),
        parseFloat(styles.paddingTop));
}

SimplePromise = Utilities.createClass(
    function()
    {
        this._chainedPromise = null;
        this._callback = null;
    }, {

    then: function (callback)
    {
        if (this._callback)
            throw "SimplePromise doesn't support multiple calls to then";

        this._callback = callback;
        this._chainedPromise = new SimplePromise;

        if (this._resolved)
            this.resolve(this._resolvedValue);

        return this._chainedPromise;
    },

    resolve: function (value)
    {
        if (!this._callback) {
            this._resolved = true;
            this._resolvedValue = value;
            return;
        }

        var result = this._callback(value);
        if (result instanceof SimplePromise) {
            var chainedPromise = this._chainedPromise;
            result.then(function (result) { chainedPromise.resolve(result); });
        } else
            this._chainedPromise.resolve(result);
    }
});

Statistics =
{
    sampleMean: function(numberOfSamples, sum)
    {
        if (numberOfSamples < 1)
            return 0;
        return sum / numberOfSamples;
    },

    // With sum and sum of squares, we can compute the sample standard deviation in O(1).
    // See https://rniwa.com/2012-11-10/sample-standard-deviation-in-terms-of-sum-and-square-sum-of-samples/
    unbiasedSampleStandardDeviation: function(numberOfSamples, sum, squareSum)
    {
        if (numberOfSamples < 2)
            return 0;
        return Math.sqrt((squareSum - sum * sum / numberOfSamples) / (numberOfSamples - 1));
    },

    geometricMean: function(values)
    {
        if (!values.length)
            return 0;
        var roots = values.map(function(value) { return  Math.pow(value, 1 / values.length); })
        return roots.reduce(function(a, b) { return a * b; });
    }
};
