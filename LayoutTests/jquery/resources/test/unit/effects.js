module("effects", { teardown: moduleTeardown });

test("sanity check", function() {
	expect(1);
	ok( jQuery("#dl:visible, #main:visible, #foo:visible").length === 3, "QUnit state is correct for testing effects" );
});

test("show()", function() {
	expect(28);

	var hiddendiv = jQuery("div.hidden");

	hiddendiv.hide().show();

	equals( hiddendiv.css("display"), "block", "Make sure a pre-hidden div is visible." );

	var div = jQuery("<div>").hide().appendTo("#main").show();

	equal( div.css("display"), "block", "Make sure pre-hidden divs show" );

	QUnit.reset();

	hiddendiv = jQuery("div.hidden");

	equal(jQuery.css( hiddendiv[0], "display"), "none", "hiddendiv is display: none");

	hiddendiv.css("display", "block");
	equal(jQuery.css( hiddendiv[0], "display"), "block", "hiddendiv is display: block");

	hiddendiv.show();
	equal(jQuery.css( hiddendiv[0], "display"), "block", "hiddendiv is display: block");

	hiddendiv.css("display","");

	var pass = true, div = jQuery("#main div");
	div.show().each(function(){
		if ( this.style.display == "none" ) pass = false;
	});
	ok( pass, "Show" );

	var speeds = {
		"null speed": null,
		"undefined speed": undefined,
		"empty string speed": "",
		"false speed": false
	};

	jQuery.each(speeds, function(name, speed) {
		pass = true;
		div.hide().show(speed).each(function() {
			if ( this.style.display == "none" ) pass = false;
		});
		ok( pass, "Show with " + name);
	});

	jQuery.each(speeds, function(name, speed) {
	pass = true;
	div.hide().show(speed, function() {
			pass = false;
		});
		ok( pass, "Show with " + name + " does not call animate callback" );
	});

	// #show-tests * is set display: none in CSS
	jQuery("#main").append('<div id="show-tests"><div><p><a href="#"></a></p><code></code><pre></pre><span></span></div><table><thead><tr><th></th></tr></thead><tbody><tr><td></td></tr></tbody></table><ul><li></li></ul></div><table id="test-table"></table>');

	var old = jQuery("#test-table").show().css("display") !== "table";
	jQuery("#test-table").remove();

	var test = {
		"div"      : "block",
		"p"        : "block",
		"a"        : "inline",
		"code"     : "inline",
		"pre"      : "block",
		"span"     : "inline",
		"table"    : old ? "block" : "table",
		"thead"    : old ? "block" : "table-header-group",
		"tbody"    : old ? "block" : "table-row-group",
		"tr"       : old ? "block" : "table-row",
		"th"       : old ? "block" : "table-cell",
		"td"       : old ? "block" : "table-cell",
		"ul"       : "block",
		"li"       : old ? "block" : "list-item"
	};

	jQuery.each(test, function(selector, expected) {
		var elem = jQuery(selector, "#show-tests").show();
		equals( elem.css("display"), expected, "Show using correct display type for " + selector );
	});
});

test("show(Number) - other displays", function() {
	expect(15);
	QUnit.reset();
	stop();

	// #show-tests * is set display: none in CSS
	jQuery("#main").append('<div id="show-tests"><div><p><a href="#"></a></p><code></code><pre></pre><span></span></div><table><thead><tr><th></th></tr></thead><tbody><tr><td></td></tr></tbody></table><ul><li></li></ul></div><table id="test-table"></table>');

	var old = jQuery("#test-table").show().css("display") !== "table",
		num = 0;
	jQuery("#test-table").remove();

	var test = {
		"div"      : "block",
		"p"        : "block",
		"a"        : "inline",
		"code"     : "inline",
		"pre"      : "block",
		"span"     : "inline",
		"table"    : old ? "block" : "table",
		"thead"    : old ? "block" : "table-header-group",
		"tbody"    : old ? "block" : "table-row-group",
		"tr"       : old ? "block" : "table-row",
		"th"       : old ? "block" : "table-cell",
		"td"       : old ? "block" : "table-cell",
		"ul"       : "block",
		"li"       : old ? "block" : "list-item"
	};

	jQuery.each(test, function(selector, expected) {
		var elem = jQuery(selector, "#show-tests").show(1, function() {
			equals( elem.css("display"), expected, "Show using correct display type for " + selector );
			if ( ++num === 15 ) {
				start();
			}
		});
	});
});



// Supports #7397
test("Persist correct display value", function() {
	expect(3);
	QUnit.reset();
	stop();

	// #show-tests * is set display: none in CSS
	jQuery("#main").append('<div id="show-tests"><span style="position:absolute;">foo</span></div>');

	var $span = jQuery("#show-tests span"),
		displayNone = $span.css("display"),
		display = '', num = 0;

	$span.show();

	display = $span.css("display");

	$span.hide();

	$span.fadeIn(100, function() {
		equals($span.css("display"), display, "Expecting display: " + display);
		$span.fadeOut(100, function () {
			equals($span.css("display"), displayNone, "Expecting display: " + displayNone);
			$span.fadeIn(100, function() {
				equals($span.css("display"), display, "Expecting display: " + display);
				start();
			});
		});
	});
});

test("animate(Hash, Object, Function)", function() {
	expect(1);
	stop();
	var hash = {opacity: 'show'};
	var hashCopy = jQuery.extend({}, hash);
	jQuery('#foo').animate(hash, 0, function() {
		equals( hash.opacity, hashCopy.opacity, 'Check if animate changed the hash parameter' );
		start();
	});
});

test("animate negative height", function() {
	expect(1);
	stop();
	jQuery("#foo").animate({ height: -100 }, 100, function() {
		equals( this.offsetHeight, 0, "Verify height." );
		start();
	});
});

test("animate block as inline width/height", function() {
	expect(3);

	var span = jQuery("<span>").css("display", "inline-block").appendTo("body"),
		expected = span.css("display");

	span.remove();

	if ( jQuery.support.inlineBlockNeedsLayout || expected === "inline-block" ) {
		stop();

		jQuery("#foo").css({ display: "inline", width: '', height: '' }).animate({ width: 42, height: 42 }, 100, function() {
			equals( jQuery(this).css("display"), jQuery.support.inlineBlockNeedsLayout ? "inline" : "inline-block", "inline-block was set on non-floated inline element when animating width/height" );
			equals( this.offsetWidth, 42, "width was animated" );
			equals( this.offsetHeight, 42, "height was animated" );
			start();
		});

	// Browser doesn't support inline-block
	} else {
		ok( true, "Browser doesn't support inline-block" );
		ok( true, "Browser doesn't support inline-block" );
		ok( true, "Browser doesn't support inline-block" );
	}
});

test("animate native inline width/height", function() {
	expect(3);

	var span = jQuery("<span>").css("display", "inline-block").appendTo("body"),
		expected = span.css("display");

	span.remove();

	if ( jQuery.support.inlineBlockNeedsLayout || expected === "inline-block" ) {
		stop();
		jQuery("#foo").css({ display: "", width: '', height: '' })
			.append('<span>text</span>')
			.children('span')
				.animate({ width: 42, height: 42 }, 100, function() {
					equals( jQuery(this).css("display"), "inline-block", "inline-block was set on non-floated inline element when animating width/height" );
					equals( this.offsetWidth, 42, "width was animated" );
					equals( this.offsetHeight, 42, "height was animated" );
					start();
				});

	// Browser doesn't support inline-block
	} else {
		ok( true, "Browser doesn't support inline-block" );
		ok( true, "Browser doesn't support inline-block" );
		ok( true, "Browser doesn't support inline-block" );
	}
});

test("animate block width/height", function() {
	expect(3);
	stop();
	jQuery("#foo").css({ display: "block", width: 20, height: 20 }).animate({ width: 42, height: 42 }, 100, function() {
		equals( jQuery(this).css("display"), "block", "inline-block was not set on block element when animating width/height" );
		equals( this.offsetWidth, 42, "width was animated" );
		equals( this.offsetHeight, 42, "height was animated" );
		start();
	});
});

test("animate table width/height", function() {
	expect(1);
	stop();

	var displayMode = jQuery("#table").css("display") !== "table" ? "block" : "table";

	jQuery("#table").animate({ width: 42, height: 42 }, 100, function() {
		equals( jQuery(this).css("display"), displayMode, "display mode is correct" );
		start();
	});
});

test("animate table-row width/height", function() {
	expect(3);
	stop();
	var tr = jQuery("#table")
		.attr({ "cellspacing": 0, "cellpadding": 0, "border": 0 })
		.html("<tr style='height:42px;'><td style='padding:0;'><div style='width:20px;height:20px;'></div></td></tr>")
		.find("tr");

	// IE<8 uses “block” instead of the correct display type
	var displayMode = tr.css("display") !== "table-row" ? "block" : "table-row";

	tr.animate({ width: 10, height: 10 }, 100, function() {
		equals( jQuery(this).css("display"), displayMode, "display mode is correct" );
		equals( this.offsetWidth, 20, "width animated to shrink wrap point" );
		equals( this.offsetHeight, 20, "height animated to shrink wrap point" );
		start();
	});
});

test("animate table-cell width/height", function() {
	expect(3);
	stop();
	var td = jQuery("#table")
		.attr({ "cellspacing": 0, "cellpadding": 0, "border": 0 })
		.html("<tr><td style='width:42px;height:42px;padding:0;'><div style='width:20px;height:20px;'></div></td></tr>")
		.find("td");

	// IE<8 uses “block” instead of the correct display type
	var displayMode = td.css("display") !== "table-cell" ? "block" : "table-cell";

	td.animate({ width: 10, height: 10 }, 100, function() {
		equals( jQuery(this).css("display"), displayMode, "display mode is correct" );
		equals( this.offsetWidth, 20, "width animated to shrink wrap point" );
		equals( this.offsetHeight, 20, "height animated to shrink wrap point" );
		start();
	});
});

test("animate resets overflow-x and overflow-y when finished", function() {
	expect(2);
	stop();
	jQuery("#foo")
		.css({ display: "block", width: 20, height: 20, overflowX: "visible", overflowY: "auto" })
		.animate({ width: 42, height: 42 }, 100, function() {
			equals( this.style.overflowX, "visible", "overflow-x is visible" );
			equals( this.style.overflowY, "auto", "overflow-y is auto" );
			start();
		});
});

/* // This test ends up being flaky depending upon the CPU load
test("animate option (queue === false)", function () {
	expect(1);
	stop();

	var order = [];

	var $foo = jQuery("#foo");
	$foo.animate({width:'100px'}, 3000, function () {
		// should finish after unqueued animation so second
		order.push(2);
		same( order, [ 1, 2 ], "Animations finished in the correct order" );
		start();
	});
	$foo.animate({fontSize:'2em'}, {queue:false, duration:10, complete:function () {
		// short duration and out of queue so should finish first
		order.push(1);
	}});
});
*/

test("animate with no properties", function() {
	expect(2);

	var divs = jQuery("div"), count = 0;

	divs.animate({}, function(){
		count++;
	});

	equals( divs.length, count, "Make sure that callback is called for each element in the set." );

	stop();

	var foo = jQuery("#foo");

	foo.animate({});
	foo.animate({top: 10}, 100, function(){
		ok( true, "Animation was properly dequeued." );
		start();
	});
});

test("animate duration 0", function() {
	expect(11);

	stop();

	var $elems = jQuery([{ a:0 },{ a:0 }]), counter = 0;

	equals( jQuery.timers.length, 0, "Make sure no animation was running from another test" );

	$elems.eq(0).animate( {a:1}, 0, function(){
		ok( true, "Animate a simple property." );
		counter++;
	});

	// Failed until [6115]
	equals( jQuery.timers.length, 0, "Make sure synchronic animations are not left on jQuery.timers" );

	equals( counter, 1, "One synchronic animations" );

	$elems.animate( { a:2 }, 0, function(){
		ok( true, "Animate a second simple property." );
		counter++;
	});

	equals( counter, 3, "Multiple synchronic animations" );

	$elems.eq(0).animate( {a:3}, 0, function(){
		ok( true, "Animate a third simple property." );
		counter++;
	});
	$elems.eq(1).animate( {a:3}, 200, function(){
		counter++;
		// Failed until [6115]
		equals( counter, 5, "One synchronic and one asynchronic" );
		start();
	});

	var $elem = jQuery("<div />");
	$elem.show(0, function(){
		ok(true, "Show callback with no duration");
	});
	$elem.hide(0, function(){
		ok(true, "Hide callback with no duration");
	});

	// manually clean up detached elements
	$elem.remove();
});

test("animate hyphenated properties", function(){
	expect(1);
	stop();

	jQuery("#foo")
		.css("font-size", 10)
		.animate({"font-size": 20}, 200, function(){
			equals( this.style.fontSize, "20px", "The font-size property was animated." );
			start();
		});
});

test("animate non-element", function(){
	expect(1);
	stop();

	var obj = { test: 0 };

	jQuery(obj).animate({test: 200}, 200, function(){
		equals( obj.test, 200, "The custom property should be modified." );
		start();
	});
});

test("stop()", function() {
	expect(3);
	stop();

	var $foo = jQuery("#foo");
	var w = 0;
	$foo.hide().width(200).width();

	$foo.animate({ width:'show' }, 1000);
	setTimeout(function(){
		var nw = $foo.width();
		notEqual( nw, w, "An animation occurred " + nw + "px " + w + "px");
		$foo.stop();

		nw = $foo.width();
		notEqual( nw, w, "Stop didn't reset the animation " + nw + "px " + w + "px");
		setTimeout(function(){
			$foo.removeData();
			$foo.removeData(undefined, true);
			equals( nw, $foo.width(), "The animation didn't continue" );
			start();
		}, 100);
	}, 100);
});

test("stop() - several in queue", function() {
	expect(3);
	stop();

	var $foo = jQuery("#foo");
	var w = 0;
	$foo.hide().width(200).width();

	$foo.animate({ width:'show' }, 1000);
	$foo.animate({ width:'hide' }, 1000);
	$foo.animate({ width:'show' }, 1000);
	setTimeout(function(){
		equals( $foo.queue().length, 3, "All 3 still in the queue" );
		var nw = $foo.width();
		notEqual( nw, w, "An animation occurred " + nw + "px " + w + "px");
		$foo.stop();

		nw = $foo.width();
		notEqual( nw, w, "Stop didn't reset the animation " + nw + "px " + w + "px");

		$foo.stop(true);
		start();
	}, 100);
});

test("stop(clearQueue)", function() {
	expect(4);
	stop();

	var $foo = jQuery("#foo");
	var w = 0;
	$foo.hide().width(200).width();

	$foo.animate({ width:'show' }, 1000);
	$foo.animate({ width:'hide' }, 1000);
	$foo.animate({ width:'show' }, 1000);
	setTimeout(function(){
		var nw = $foo.width();
		ok( nw != w, "An animation occurred " + nw + "px " + w + "px");
		$foo.stop(true);

		nw = $foo.width();
		ok( nw != w, "Stop didn't reset the animation " + nw + "px " + w + "px");

		equals( $foo.queue().length, 0, "The animation queue was cleared" );
		setTimeout(function(){
			equals( nw, $foo.width(), "The animation didn't continue" );
			start();
		}, 100);
	}, 100);
});

test("stop(clearQueue, gotoEnd)", function() {
	expect(1);
	stop();

	var $foo = jQuery("#foo");
	var w = 0;
	$foo.hide().width(200).width();

	$foo.animate({ width:'show' }, 1000);
	$foo.animate({ width:'hide' }, 1000);
	$foo.animate({ width:'show' }, 1000);
	$foo.animate({ width:'hide' }, 1000);
	setTimeout(function(){
		var nw = $foo.width();
		ok( nw != w, "An animation occurred " + nw + "px " + w + "px");
		$foo.stop(false, true);

		nw = $foo.width();
		// Disabled, being flaky
		//equals( nw, 1, "Stop() reset the animation" );

		setTimeout(function(){
			// Disabled, being flaky
			//equals( $foo.queue().length, 2, "The next animation continued" );
			$foo.stop(true);
			start();
		}, 100);
	}, 100);
});

test("toggle()", function() {
	expect(6);
	var x = jQuery("#foo");
	ok( x.is(":visible"), "is visible" );
	x.toggle();
	ok( x.is(":hidden"), "is hidden" );
	x.toggle();
	ok( x.is(":visible"), "is visible again" );

	x.toggle(true);
	ok( x.is(":visible"), "is visible" );
	x.toggle(false);
	ok( x.is(":hidden"), "is hidden" );
	x.toggle(true);
	ok( x.is(":visible"), "is visible again" );
});

jQuery.checkOverflowDisplay = function(){
	var o = jQuery.css( this, "overflow" );

	equals(o, "visible", "Overflow should be visible: " + o);
	equals(jQuery.css( this, "display" ), "inline", "Display shouldn't be tampered with.");

	start();
}

test( "jQuery.fx.prototype.cur()", 6, function() {
	var div = jQuery( "<div></div>" ).appendTo( "#main" ).css({
			color: "#ABC",
			border: "5px solid black",
			left: "auto",
			marginBottom: "-11000px"
		})[0];

	equals(
		( new jQuery.fx( div, {}, "color" ) ).cur(),
		jQuery.css( div, "color" ),
		"Return the same value as jQuery.css for complex properties (bug #7912)"
	);

	strictEqual(
		( new jQuery.fx( div, {}, "borderLeftWidth" ) ).cur(),
		5,
		"Return simple values parsed as Float"
	);

	// backgroundPosition actually returns 0% 0% in most browser
	// this fakes a "" return
	jQuery.cssHooks.backgroundPosition = {
		get: function() {
			ok( true, "hook used" );
			return "";
		}
	};

	strictEqual(
		( new jQuery.fx( div, {}, "backgroundPosition" ) ).cur(),
		0,
		"Return 0 when jQuery.css returns an empty string"
	);

	delete jQuery.cssHooks.backgroundPosition;

	strictEqual(
		( new jQuery.fx( div, {}, "left" ) ).cur(),
		0,
		"Return 0 when jQuery.css returns 'auto'"
	);

	equals(
		( new jQuery.fx( div, {}, "marginBottom" ) ).cur(),
		-11000,
		"support negative values < -10000 (bug #7193)"
	);
});

test("JS Overflow and Display", function() {
	expect(2);
	stop();
	jQuery.makeTest( "JS Overflow and Display" )
		.addClass("widewidth")
		.css({ overflow: "visible", display: "inline" })
		.addClass("widewidth")
		.text("Some sample text.")
		.before("text before")
		.after("text after")
		.animate({ opacity: 0.5 }, "slow", jQuery.checkOverflowDisplay);
});

test("CSS Overflow and Display", function() {
	expect(2);
	stop();
	jQuery.makeTest( "CSS Overflow and Display" )
		.addClass("overflow inline")
		.addClass("widewidth")
		.text("Some sample text.")
		.before("text before")
		.after("text after")
		.animate({ opacity: 0.5 }, "slow", jQuery.checkOverflowDisplay);
});

jQuery.each( {
	"CSS Auto": function(elem,prop){
		jQuery(elem).addClass("auto" + prop)
			.text("This is a long string of text.");
		return "";
	},
	"JS Auto": function(elem,prop){
		jQuery(elem).css(prop,"")
			.text("This is a long string of text.");
		return "";
	},
	"CSS 100": function(elem,prop){
		jQuery(elem).addClass("large" + prop);
		return "";
	},
	"JS 100": function(elem,prop){
		jQuery(elem).css(prop,prop == "opacity" ? 1 : "100px");
		return prop == "opacity" ? 1 : 100;
	},
	"CSS 50": function(elem,prop){
		jQuery(elem).addClass("med" + prop);
		return "";
	},
	"JS 50": function(elem,prop){
		jQuery(elem).css(prop,prop == "opacity" ? 0.50 : "50px");
		return prop == "opacity" ? 0.5 : 50;
	},
	"CSS 0": function(elem,prop){
		jQuery(elem).addClass("no" + prop);
		return "";
	},
	"JS 0": function(elem,prop){
		jQuery(elem).css(prop,prop == "opacity" ? 0 : "0px");
		return 0;
	}
}, function(fn, f){
	jQuery.each( {
		"show": function(elem,prop){
			jQuery(elem).hide().addClass("wide"+prop);
			return "show";
		},
		"hide": function(elem,prop){
			jQuery(elem).addClass("wide"+prop);
			return "hide";
		},
		"100": function(elem,prop){
			jQuery(elem).addClass("wide"+prop);
			return prop == "opacity" ? 1 : 100;
		},
		"50": function(elem,prop){
			return prop == "opacity" ? 0.50 : 50;
		},
		"0": function(elem,prop){
			jQuery(elem).addClass("noback");
			return 0;
		}
	}, function(tn, t){
		test(fn + " to " + tn, function() {
			var elem = jQuery.makeTest( fn + " to " + tn );

			var t_w = t( elem, "width" );
			var f_w = f( elem, "width" );
			var t_h = t( elem, "height" );
			var f_h = f( elem, "height" );
			var t_o = t( elem, "opacity" );
			var f_o = f( elem, "opacity" );

			var num = 0;

			if ( t_h == "show" ) num++;
			if ( t_w == "show" ) num++;
			if ( t_w == "hide"||t_w == "show" ) num++;
			if ( t_h == "hide"||t_h == "show" ) num++;
			if ( t_o == "hide"||t_o == "show" ) num++;
			if ( t_w == "hide" ) num++;
			if ( t_o.constructor == Number ) num += 2;
			if ( t_w.constructor == Number ) num += 2;
			if ( t_h.constructor == Number ) num +=2;

			expect(num);
			stop();

			var anim = { width: t_w, height: t_h, opacity: t_o };

			elem.animate(anim, 50, function(){
				if ( t_w == "show" )
					equals( this.style.display, "block", "Showing, display should block: " + this.style.display);

				if ( t_w == "hide"||t_w == "show" )
					ok(f_w === "" ? this.style.width === f_w : this.style.width.indexOf(f_w) === 0, "Width must be reset to " + f_w + ": " + this.style.width);

				if ( t_h == "hide"||t_h == "show" )
					ok(f_h === "" ? this.style.height === f_h : this.style.height.indexOf(f_h) === 0, "Height must be reset to " + f_h + ": " + this.style.height);

				var cur_o = jQuery.style(this, "opacity");

				if ( t_o == "hide" || t_o == "show" )
					equals(cur_o, f_o, "Opacity must be reset to " + f_o + ": " + cur_o);

				if ( t_w == "hide" )
					equals(this.style.display, "none", "Hiding, display should be none: " + this.style.display);

				if ( t_o.constructor == Number ) {
					equals(cur_o, t_o, "Final opacity should be " + t_o + ": " + cur_o);

					ok(jQuery.css(this, "opacity") != "" || cur_o == t_o, "Opacity should be explicitly set to " + t_o + ", is instead: " + cur_o);
				}

				if ( t_w.constructor == Number ) {
					equals(this.style.width, t_w + "px", "Final width should be " + t_w + ": " + this.style.width);

					var cur_w = jQuery.css(this,"width");

					ok(this.style.width != "" || cur_w == t_w, "Width should be explicitly set to " + t_w + ", is instead: " + cur_w);
				}

				if ( t_h.constructor == Number ) {
					equals(this.style.height, t_h + "px", "Final height should be " + t_h + ": " + this.style.height);

					var cur_h = jQuery.css(this,"height");

					ok(this.style.height != "" || cur_h == t_h, "Height should be explicitly set to " + t_h + ", is instead: " + cur_w);
				}

				if ( t_h == "show" ) {
					var old_h = jQuery.css(this, "height");
					jQuery(this).append("<br/>Some more text<br/>and some more...");

					if ( /Auto/.test( fn ) ) {
						notEqual(jQuery.css(this, "height"), old_h, "Make sure height is auto.");
					} else {
						equals(jQuery.css(this, "height"), old_h, "Make sure height is not auto.");
					}
				}

				// manually remove generated element
				jQuery(this).remove();

				start();
			});
		});
	});
});

jQuery.fn.saveState = function(hiddenOverflow){
	var check = ['opacity','height','width','display','overflow'];
	expect(check.length);

	stop();
	return this.each(function(){
		var self = this;
		self.save = {};
		jQuery.each(check, function(i,c){
			self.save[c] = c === "overflow" && hiddenOverflow ? "hidden" : self.style[ c ] || jQuery.css(self,c);
		});
	});
};

jQuery.checkState = function(){
	var self = this;
	jQuery.each(this.save, function(c,v){
		var cur = self.style[ c ] || jQuery.css(self, c);
		equals( cur, v, "Make sure that " + c + " is reset (Old: " + v + " Cur: " + cur + ")");
	});

	// manually clean data on modified element
	jQuery.removeData(this, 'olddisplay', true);

	start();
}

// Chaining Tests
test("Chain fadeOut fadeIn", function() {
	jQuery('#fadein div').saveState().fadeOut('fast').fadeIn('fast',jQuery.checkState);
});
test("Chain fadeIn fadeOut", function() {
	jQuery('#fadeout div').saveState().fadeIn('fast').fadeOut('fast',jQuery.checkState);
});

test("Chain hide show", function() {
	jQuery('#show div').saveState(jQuery.support.shrinkWrapBlocks).hide('fast').show('fast',jQuery.checkState);
});
test("Chain show hide", function() {
	jQuery('#hide div').saveState(jQuery.support.shrinkWrapBlocks).show('fast').hide('fast',jQuery.checkState);
});
test("Chain show hide with easing and callback", function() {
	jQuery('#hide div').saveState().show('fast').hide('fast','linear',jQuery.checkState);
});

test("Chain toggle in", function() {
	jQuery('#togglein div').saveState(jQuery.support.shrinkWrapBlocks).toggle('fast').toggle('fast',jQuery.checkState);
});
test("Chain toggle out", function() {
	jQuery('#toggleout div').saveState(jQuery.support.shrinkWrapBlocks).toggle('fast').toggle('fast',jQuery.checkState);
});
test("Chain toggle out with easing and callback", function() {
	jQuery('#toggleout div').saveState(jQuery.support.shrinkWrapBlocks).toggle('fast').toggle('fast','linear',jQuery.checkState);
});
test("Chain slideDown slideUp", function() {
	jQuery('#slidedown div').saveState(jQuery.support.shrinkWrapBlocks).slideDown('fast').slideUp('fast',jQuery.checkState);
});
test("Chain slideUp slideDown", function() {
	jQuery('#slideup div').saveState(jQuery.support.shrinkWrapBlocks).slideUp('fast').slideDown('fast',jQuery.checkState);
});
test("Chain slideUp slideDown with easing and callback", function() {
	jQuery('#slideup div').saveState(jQuery.support.shrinkWrapBlocks).slideUp('fast').slideDown('fast','linear',jQuery.checkState);
});

test("Chain slideToggle in", function() {
	jQuery('#slidetogglein div').saveState(jQuery.support.shrinkWrapBlocks).slideToggle('fast').slideToggle('fast',jQuery.checkState);
});
test("Chain slideToggle out", function() {
	jQuery('#slidetoggleout div').saveState(jQuery.support.shrinkWrapBlocks).slideToggle('fast').slideToggle('fast',jQuery.checkState);
});

test("Chain fadeToggle in", function() {
	jQuery('#fadetogglein div').saveState().fadeToggle('fast').fadeToggle('fast',jQuery.checkState);
});
test("Chain fadeToggle out", function() {
	jQuery('#fadetoggleout div').saveState().fadeToggle('fast').fadeToggle('fast',jQuery.checkState);
});

test("Chain fadeTo 0.5 1.0 with easing and callback)", function() {
	jQuery('#fadeto div').saveState().fadeTo('fast',0.5).fadeTo('fast',1.0,'linear',jQuery.checkState);
});

jQuery.makeTest = function( text ){
	var elem = jQuery("<div></div>")
		.attr("id", "test" + jQuery.makeTest.id++)
		.addClass("box");

	jQuery("<h4></h4>")
		.text( text )
		.appendTo("#fx-tests")
		.after( elem );

	return elem;
}

jQuery.makeTest.id = 1;

test("jQuery.show('fast') doesn't clear radio buttons (bug #1095)", function () {
	expect(4);
	stop();

	var $checkedtest = jQuery("#checkedtest");
	// IE6 was clearing "checked" in jQuery(elem).show("fast");
	$checkedtest.hide().show("fast", function() {
		ok( !! jQuery(":radio:first", $checkedtest).attr("checked"), "Check first radio still checked." );
		ok( ! jQuery(":radio:last", $checkedtest).attr("checked"), "Check last radio still NOT checked." );
		ok( !! jQuery(":checkbox:first", $checkedtest).attr("checked"), "Check first checkbox still checked." );
		ok( ! jQuery(":checkbox:last", $checkedtest).attr("checked"), "Check last checkbox still NOT checked." );
		start();
	});
});

test("animate with per-property easing", function(){

	expect(3);
	stop();

	var _test1_called = false;
	var _test2_called = false;
	var _default_test_called = false;

	jQuery.easing['_test1'] = function() {
		_test1_called = true;
	};

	jQuery.easing['_test2'] = function() {
		_test2_called = true;
	};

	jQuery.easing['_default_test'] = function() {
		_default_test_called = true;
	};

	jQuery({a:0,b:0,c:0}).animate({
		a: [100, '_test1'],
		b: [100, '_test2'],
		c: 100
	}, 400, '_default_test', function(){
		start();
		ok(_test1_called, "Easing function (1) called");
		ok(_test2_called, "Easing function (2) called");
		ok(_default_test_called, "Easing function (_default) called");
	});

});

test("hide hidden elements (bug #7141)", function() {
	expect(3);
	QUnit.reset();

	var div = jQuery("<div style='display:none'></div>").appendTo("#main");
	equals( div.css("display"), "none", "Element is hidden by default" );
	div.hide();
	ok( !jQuery._data(div, "olddisplay"), "olddisplay is undefined after hiding an already-hidden element" );
	div.show();
	equals( div.css("display"), "block", "Show a double-hidden element" );

	div.remove();
});

test("hide hidden elements, with animation (bug #7141)", function() {
	expect(3);
	QUnit.reset();
	stop();

	var div = jQuery("<div style='display:none'></div>").appendTo("#main");
	equals( div.css("display"), "none", "Element is hidden by default" );
	div.hide(1, function () {
		ok( !jQuery._data(div, "olddisplay"), "olddisplay is undefined after hiding an already-hidden element" );
		div.show(1, function () {
			equals( div.css("display"), "block", "Show a double-hidden element" );
			start();
		});
	});
});

test("animate unit-less properties (#4966)", 2, function() {
	stop();
	var div = jQuery( "<div style='z-index: 0; position: absolute;'></div>" ).appendTo( "#main" );
	equal( div.css( "z-index" ), "0", "z-index is 0" );
	div.animate({ zIndex: 2 }, function() {
		equal( div.css( "z-index" ), "2", "z-index is 2" );
		start();
	});
});
