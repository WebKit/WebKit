'use strict';

description("Tests that we don't throw a type error in strict mode when assigning to an instance attribute that shadows navigator.geolocation. See <a href='https://bugs.webkit.org/show_bug.cgi?id=133559'>https://bugs.webkit.org/show_bug.cgi?id=133559</a>");

function createObjectWithPrototype(prototype)
{
    function F() {};
    F.prototype = prototype;
    return new F();
}

var myNavigator = createObjectWithPrototype(window.navigator)
shouldBe("myNavigator.geolocation", "navigator.geolocation");
shouldNotThrow("myNavigator.geolocation = 1");
shouldBe("myNavigator.geolocation", "navigator.geolocation");

window.jsTestIsAsync = false;
