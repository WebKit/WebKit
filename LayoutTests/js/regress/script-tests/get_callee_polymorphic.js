var Class = {
    create: function() {
        return function() { 
            this.initialize.apply(this, arguments);
        };
    }
};

var sum = 0;

var init = function(a, b) { sum += a + b; };

var Class1 = Class.create();
Class1.prototype = {
    initialize: init
};
var Class2 = Class.create();
Class2.prototype = {
    initialize: init
};
var Class3 = Class.create();
Class3.prototype = {
    initialize: init
};

for (var i = 0; i < 1000; i++) {
    for (var j = 0; j < 100; j++) {
        var newObject;
        if (j % 3 == 0)
            newObject = new Class1(2, 3);
        else if (j % 3 == 1)
            newObject = new Class2(2, 3);
        else
            newObject = new Class3(2, 3);
    }
}

if (sum != 5 * 100 * 1000)
    throw "Error: incorrect sum. Expected " + (5 * 100 * 1000) + " but got " + sum + ".";
