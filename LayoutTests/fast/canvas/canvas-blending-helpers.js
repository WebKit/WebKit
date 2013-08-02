var blendModes = ["source-over", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn",
                  "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"];

// Helper functions for separate blend mode

var separateBlendmodes = ["normal", "multiply", "screen", "overlay",
                          "darken", "lighten", "colorDodge","colorBurn",
                          "hardLight", "softLight", "difference", "exclusion"];

var separateBlendFunctions = {
    normal: function(b, s) {
        return s;
    },
    multiply: function(b, s) {
        return b * s;
    },
    screen: function(b, s) {
        return b + s - b * s;
    },
    overlay: function(b, s) {
        return separateBlendFunctions.hardLight(b, s);
    },
    darken: function(b, s) {
        return Math.min(b, s);
    },
    lighten: function(b, s) {
        return Math.max(b, s);
    },
    colorDodge: function(b, s) {
        if(b == 1)
            return 1;
        return Math.min(1, s / (1 - b));
    },
    colorBurn: function(b, s) {
        if(s == 0)
            return 0;
        return 1 - Math.min(1, (1 - b) / s);
    },
    hardLight: function(b, s) {
        if(s <= 0.5)
            return separateBlendFunctions.multiply(s, 2 * b);

        return separateBlendFunctions.screen(s, 2 * b - 1);
    },
    // soft light as well as the non separate blend modes seem to be broken, as stated in
    // https://bugs.webkit.org/show_bug.cgi?id=105609.
    // Radar 12922671.
    softLight: function(b, s) {
        return s;
        // var c = 0;
        // if(b <= 0.25)
        //     c = ((16 * b - 12) * b + 4) * b;
        // else
        //     c = Math.sqrt(b);

        // if(s <= 0.5)
        //     return b - (1 - 2 * s) * b * (1 - b);

        // return b + (2  * s - 1) * (c - b);
    },
    difference: function(b, s) {
        return Math.abs(b - s);
    },
    exclusion: function(b, s) {
        return s + b - 2 * b * s;
    }
};

function applyBlendMode(b, s, blendFunc) {
    var resultedColor = [0, 0, 0, 255];
    for (var i = 0; i < 3; ++i)
        resultedColor[i] = 255 * (s[3] * (1 - b[3]) * s[i] + b[3] * s[3] * blendFunc(b[i], s[i]) + (1 - s[3]) * b[3] * b[i]);
    return resultedColor;
}


// Helper functions for nonseparate blend modes

var nonSeparateBlendModes = ["hue", "saturation", "color", "luminosity"];

function luminosity(c) {
    return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2];
}

function clipColor(c) {
    var l = luminosity(c);
    var n = Math.min(c[0], c[1], c[2]);
    var x = Math.max(c[0], c[1], c[2]);

    if (n < 0) {
        c[0] = l + (((c[0] - l) * l) / (l - n));
        c[1] = l + (((c[1] - l) * l) / (l - n));
        c[2] = l + (((c[1] - l) * l) / (l - n));
    }

    if (x > 1) {
        c[0] = l + (((c[0] - l) * (1 - l)) / (x - l));
        c[1] = l + (((c[1] - l) * (1 - l)) / (x - l));
        c[2] = l + (((c[2] - l) * (1 - l)) / (x - l));
    }

    return c;
}

function setLuminosity(c, l) {
    var d = l - luminosity(c);
    c[0] += d;
    c[1] += d;
    c[2] += d;
    return clipColor(c);
}

function saturation(c) {
    return Math.max(c[0], c[1], c[2]) - Math.min(c[0], c[1], c[2]);
}

function setSaturation(c, s) {
    var max = Math.max(c[0], c[1], c[2]);
    var min = Math.min(c[0], c[1], c[2]);
    var index_max = -1;
    var index_min = -1;

    for (var i = 0; i < 3; ++i) {
        if (c[i] == min && index_min == -1) {
            index_min = i;
            continue;
        }
        if (c[i] == max && index_max == -1)
            index_max = i;
    }
    var index_mid = 3 - index_max - index_min;
    var mid = c[index_mid];


    if (max > min) {
        mid = (((mid - min) * s) / (max - min));
        max = s;
    } else {
        mid = 0;
        max = 0;
    }
    min = 0;

    var newColor = [0, 0, 0];

    newColor[index_min] = min;
    newColor[index_mid] = mid;
    newColor[index_max] = max;

    return newColor;
}
// soft light as well as the non separate blend modes seem to be broken, as stated in
// https://bugs.webkit.org/show_bug.cgi?id=105609.
// Radar 12922671.
var nonSeparateBlendFunctions = {
    hue: function(b, s) {
        return s;
        // var bCopy = [b[0], b[1], b[2]];
        // var sCopy = [s[0], s[1], s[2]];
        // return setLuminosity(setSaturation(sCopy, saturation(bCopy)), luminosity(bCopy));
    },
    saturation: function(b, s) {
        return s;
        // var bCopy = [b[0], b[1], b[2]];
        // var sCopy = [s[0], s[1], s[2]];
        // return setLuminosity(setSaturation(bCopy, saturation(sCopy)), luminosity(bCopy));
    },
    color: function(b, s) {
        return s;
        // var bCopy = [b[0], b[1], b[2]];
        // var sCopy = [s[0], s[1], s[2]];
        // return setLuminosity(sCopy, luminosity(bCopy));
    },
    luminosity: function(b, s) {
        return s;
        // var bCopy = [b[0], b[1], b[2]];
        // var sCopy = [s[0], s[1], s[2]];
        // return setLuminosity(bCopy, luminosity(sCopy));
    }
};

// Helper functions for drawing in canvas tests

function drawColorInContext(color, context) {
    context.fillStyle = color;
    context.fillRect(0, 0, 10, 10);
}

function drawBackdropColorInContext(context) {
    drawColorInContext("rgba(129, 255, 129, 1)", context);
}

function drawSourceColorInContext(context) {
    drawColorInContext("rgba(255, 129, 129, 1)", context);
}

function fillPathWithColorInContext(color, context) {
    context.fillStyle = color;
    context.lineTo(0, 10);
    context.lineTo(10, 10);
    context.lineTo(10, 0);
    context.lineTo(0, 0);
    context.fill();
}

function fillPathWithBackdropInContext(context) {
    fillPathWithColorInContext("rgba(129, 255, 129, 1)", context);
}

function fillPathWithSourceInContext(context) {
    fillPathWithColorInContext("rgba(255, 129, 129, 1)", context);
}

function applyTransformsToContext(context) {
    context.translate(1, 1);
    context.rotate(Math.PI / 2);
    context.scale(2, 2);
}

function drawBackdropColorWithShadowInContext(context) {
    context.save();
    context.shadowOffsetX = 2;
    context.shadowOffsetY = 2;
    context.shadowColor = 'rgba(192, 192, 192, 1)';
    drawBackdropColorInContext(context);
    context.restore();
}

function drawSourceColorRectOverShadow(context) {
    context.fillStyle = "rgba(255, 129, 129, 1)";
    context.fillRect(0, 0, 12, 12);
}

function drawColorImageInContext(color, context, callback) {
    var cvs = document.createElement("canvas");
    var ctx = cvs.getContext("2d");
    drawColorInContext(color, ctx);
    var imageURL = cvs.toDataURL();

    var backdropImage = new Image();
    backdropImage.onload = function() {
        context.drawImage(this, 0, 0);
        callback();
    }
    backdropImage.src = imageURL;
}

function drawBackdropColorImageInContext(context, callback) {
    drawColorImageInContext("rgba(129, 255, 129, 1)", context, callback);
}

function drawSourceColorImageInContext(context, callback) {
    drawColorImageInContext("rgba(255, 129, 129, 1)", context, callback);
}

function drawColorPatternInContext(color, context, callback) {
    var cvs = document.createElement("canvas");
    var ctx = cvs.getContext("2d");
    drawColorInContext(color, ctx);
    var imageURL = cvs.toDataURL();

    var backdropImage = new Image();
    backdropImage.onload = function() {
    var pattern = context.createPattern(backdropImage, 'repeat');
        context.rect(0, 0, 10, 10);
        context.fillStyle = pattern;
        context.fill();
        callback();
    }
    backdropImage.src = imageURL;
}

function drawBackdropColorPatternInContext(context, callback) {
    drawColorPatternInContext("rgba(129, 255, 129, 1)", context, callback);
}

function drawSourceColorPatternInContext(context, callback) {
    drawColorPatternInContext("rgba(255, 129, 129, 1)", context, callback);
}

function drawGradientInContext(color1, context) {
    var grad = context.createLinearGradient(0, 0, 10, 10);
    grad.addColorStop(0, color1);
    grad.addColorStop(1, color1);
    context.fillStyle = grad;
    context.fillRect(0, 0, 10, 10);
}

function drawBackdropColorGradientInContext(context) {
    drawGradientInContext("rgba(129, 255, 129, 1)", context);
}

function drawSourceColorGradientInContext(context) {
    drawGradientInContext("rgba(255, 129, 129, 1)", context);
}

function blendColors(backdrop, source, blendModeIndex) {
    if (blendModeIndex < separateBlendmodes.length)
        return separateBlendColors(backdrop, source, blendModeIndex);
    return nonSeparateBlendColors(backdrop, source, blendModeIndex - separateBlendmodes.length);
}

function separateBlendColors(backdrop, source, blendModeIndex) {
    return applyBlendMode(backdrop, source, separateBlendFunctions[separateBlendmodes[blendModeIndex]]);
}

function nonSeparateBlendColors(backdrop, source, blendModeIndex) {
    var expectedColor = nonSeparateBlendFunctions[nonSeparateBlendModes[blendModeIndex]](backdrop, source);
    for (var i = 0; i < 3; ++i)
        expectedColor[i] = source[3] * (1 - backdrop[3]) * source[i] + source[3] * backdrop[3] * expectedColor[i] + (1 - source[3]) * backdrop[3] * backdrop[i];
    return [Math.round(255 * expectedColor[0]), Math.round(255 * expectedColor[1]), Math.round(255 * expectedColor[2]), 255];
}
