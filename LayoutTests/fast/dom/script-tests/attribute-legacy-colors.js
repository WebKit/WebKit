description("This test ensures that legacy color attributes are parsed properly.");

shouldBe("document.body.bgColor='';getComputedStyle(document.body).backgroundColor;", "'rgba(0, 0, 0, 0)'");
shouldBe("document.body.bgColor='transparent';getComputedStyle(document.body).backgroundColor;", "'rgba(0, 0, 0, 0)'");
shouldBe("document.body.bgColor=' transparent ';getComputedStyle(document.body).backgroundColor;", "'rgba(0, 0, 0, 0)'");
(function(){
var tests = [
	{'test':'red', 'expected':[255, 0, 0]},
	{'test':' red ', 'expected':[255, 0, 0]},
	{'test':'#f00', 'expected':[255, 0, 0]},
	{'test':' #f00 ', 'expected':[255, 0, 0]},
	{'test':'#ff0000', 'expected':[255, 0, 0]},
	{'test':' #ff0000 ', 'expected':[255, 0, 0]},
	{'test':'#fzz', 'expected':[15, 0, 0]},
	{'test':'#ffzzzz', 'expected':[255, 0, 0]},
	{'test':'f00', 'expected':[15, 0, 0]},
	{'test':'ff0000', 'expected':[255, 0, 0]},
	{'test':'#00000000', 'expected':[0, 0, 0]},
	{'test':'foo', 'expected':[15, 0, 0]},
	{'test':'cheese', 'expected':[192, 238, 14]},
	{'test':'ff򀿿ff', 'expected':[255, 0, 255]},
	{'test':'f򀿿f', 'expected':[240, 15, 0]},
	{'test':'rgb(255, 0, 0)', 'expected':[0, 85, 0]},
	{'test':'rgba(255,255,255,50%)', 'expected':[0,80,85]},
	{'test':'hsl(180,100%,50%)', 'expected':[0,1,80]},
	{'test':'hsla(180,100%,50%,50%)', 'expected':[0,16,5]},
	{'test':'currentColor', 'expected':[192,224,0]},
	{'test':'550000001155000000115500000011', 'expected':[17, 17, 17]},
	{'test':'550000000155000000015500000001', 'expected':[1, 1, 1]},
	{'test':'550000000055000000005500000000', 'expected':[0, 0, 0]},
	{'test':'550020001155000000115500000011', 'expected':[32, 0, 0]},
	{'test':'55򀿿20򀿿1155򀿿򀿿00115500򀿿0011', 'expected':[32, 0, 0]},
	{'test':'#', 'expected':[0, 0, 0]},
	{'test':'#5', 'expected':[5, 0, 0]},
	{'test':'#55', 'expected':[5, 5, 0]},
	{'test':'#555', 'expected':[85, 85, 85]},
	{'test':'#5555', 'expected':[85, 85, 0]},
	{'test':'#55555', 'expected':[85, 85, 80]},
	{'test':'#555555', 'expected':[85, 85, 85]},
	{'test':'#5555555', 'expected':[85, 85, 80]},
	{'test':'#55555555', 'expected':[85, 85, 85]},
	{'test':'5', 'expected':[5, 0, 0]},
	{'test':'55', 'expected':[5, 5, 0]},
	{'test':'555', 'expected':[5, 5, 5]},
	{'test':'5555', 'expected':[85, 85, 0]},
	{'test':'55555', 'expected':[85, 85, 80]},
	{'test':'555555', 'expected':[85, 85, 85]},
	{'test':'5555555', 'expected':[85, 85, 80]},
	{'test':'55555555', 'expected':[85, 85, 85]},
	{'test':'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff000000', 'expected':[255, 255, 255]},
	{'test':'򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿򀿿ffffff', 'expected':[0, 0, 0]},
	{'test':' ', 'expected':[0, 0, 0]},
	{'test':' ffffff ', 'expected':[255, 255, 255]}
];

for(var i = 0; i < tests.length; i++) {
	var t = tests[i].test;
	var e = tests[i].expected;
	shouldBe("document.body.bgColor='" + t + "';getComputedStyle(document.body).backgroundColor;", "'rgb(" + e[0] + ", " + e[1] + ", " + e[2] + ")'");
}
})();
