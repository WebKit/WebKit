var SRITests = [];
SRITests.execute = function() {
    if (this.length > 0) {
        this.shift().execute();
    }
}

add_result_callback(function(res) {
    if (res.name.startsWith("Script: ") || res.name.startsWith("Style: ")) {
      SRITests.execute();
    }
});


var SRIScriptTest = function(pass, name, src, integrityValue, crossoriginValue) {
    this.pass = pass;
    this.name = "Script: " + name;
    this.src = src;
    this.integrityValue = integrityValue;
    this.crossoriginValue = crossoriginValue;

    this.test = async_test(this.name);

    this.queue = SRITests;
    this.queue.push(this);
}

SRIScriptTest.prototype.execute = function() {
    var test = this.test;
    var e = document.createElement("script");
    e.src = this.src;
    e.setAttribute("integrity", this.integrityValue);
    if(this.crossoriginValue) {
        e.setAttribute("crossorigin", this.crossoriginValue);
    }
    if(this.pass) {
        e.addEventListener("load", function() {test.done()});
        e.addEventListener("error", function() {
            test.step(function(){ assert_unreached("Good load fired error handler.") })
        });
    } else {
       e.addEventListener("load", function() {
            test.step(function() { assert_unreached("Bad load succeeded.") })
        });
       e.addEventListener("error", function() {test.done()});
    }
    document.body.appendChild(e);
};


var SRIStyleTest = function(pass, name, attrs, customCallback, altPassValue) {
    this.pass = pass;
    this.name = "Style: " + name;
    this.attrs = attrs || {};
    this.customCallback = customCallback || function () {};
    this.passValue = altPassValue || "rgb(255, 255, 0)";

    this.test = async_test(this.name);

    this.queue = SRITests;
    this.queue.push(this);
}

SRIStyleTest.prototype.execute = function() {
    var that = this;
    var container = document.getElementById("container");
    while (container.hasChildNodes()) {
      container.removeChild(container.firstChild);
    }

    var test = this.test;

    var div = document.createElement("div");
    div.className = "testdiv";
    var e = document.createElement("link");
    this.attrs.rel = this.attrs.rel || "stylesheet";
    for (var key in this.attrs) {
        if (this.attrs.hasOwnProperty(key)) {
            e.setAttribute(key, this.attrs[key]);
        }
    }

    if(this.pass) {
        e.addEventListener("load", function() {
            test.step(function() {
                var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
                assert_equals(background, that.passValue);
                test.done();
            });
        });
        e.addEventListener("error", function() {
            test.step(function(){ assert_unreached("Good load fired error handler.") })
        });
    } else {
        e.addEventListener("load", function() {
             test.step(function() { assert_unreached("Bad load succeeded.") })
         });
        e.addEventListener("error", function() {
            test.step(function() {
                var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
                assert_not_equals(background, that.passValue);
                test.done();
            });
        });
    }
    container.appendChild(div);
    container.appendChild(e);
    this.customCallback(e, container);
};
