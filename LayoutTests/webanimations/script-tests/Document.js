description("Web Animation API: Document interface extension test.");

var iframe = document.getElementById("iframe");
var target = document.getElementById("target");

// Check document.timeline.
shouldNotBe("document.timeline", "null");

// Returns same object each time.
shouldBe("document.timeline", "document.timeline");

// Different object for each document.
shouldNotBe("document.timeline", "iframe.contentDocument.timeline");

// All getAnimations calls should be empty.
shouldBe("document.getAnimations().length", "0");
shouldBe("target.getAnimations().length", "0");
shouldBe("iframe.contentDocument.getAnimations().length", "0");

// Check created animation.
var effect = new KeyframeEffect(target);
var animation = new Animation(effect, document.timeline);
shouldNotBe("animation", "null");

// Check getAnimations for timeline and target.
shouldBe("document.getAnimations().length", "1");
shouldBe("target.getAnimations().length", "1");

// Check getAnimations for iframe.
shouldBe("iframe.contentDocument.getAnimations().length", "0");

debug("");

successfullyParsed = true;
