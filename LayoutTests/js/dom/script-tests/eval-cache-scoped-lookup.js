description(
"Test to make sure that scoped variable access through an eval produces the correct results"
);



var str = "(function () { return a; })";

function first() {
    var a = "first"
    return eval(str)();
}

var a = "second"
function second() {
    var b = "error"
    return eval(str)();
}

function third() {
    var a = "third"
	return (function () {
		var b = "error";
	    return eval(str)();
	})()
}

function fourth() {
    eval('var a = "fourth"');
    return eval(str)();
}

function fifth() {
	"use strict";
	var a = "fifth"
	return (function() {
	    eval('var a = "error"');
	    return eval(str)();
	})();
}

function sixth() {
    var a = "sixth"
    try {
	    return eval(str)();
	} catch (e) {
	}
}

function seventh() {
    try {
    	throw "seventh"
    } catch (a) {
    	return eval(str)();
    }
}

function eighth() {
	var a = "eighth"
    try {
    	throw "error"
    } catch (b) {
    	return eval(str)();
    }
}

function nineth() {
	var a = "nineth"
	eval(str)();
    try {
    	throw "error"
    } catch (b) {
    	return eval(str)();
    }
}

function tenth() {
	var a = "error"
	eval(str)();
    try {
    	throw "tenth"
    } catch (a) {
    	return eval(str)();
    }
}

function eleventh() {
	var a = "eleventh"
    try {
    	throw "error"
    } catch (a) {
    	eval(str)();
    }
	return eval(str)();
}


shouldBe("first()", "'first'");
shouldBe("second()", "'second'");
shouldBe("third()", "'third'");
shouldBe("fourth()", "'fourth'");
shouldBe("fifth()", "'fifth'");
shouldBe("sixth()", "'sixth'");
shouldBe("seventh()", "'seventh'");
shouldBe("eighth()", "'eighth'");
shouldBe("nineth()", "'nineth'");
shouldBe("tenth()", "'tenth'");
shouldBe("eleventh()", "'eleventh'");

