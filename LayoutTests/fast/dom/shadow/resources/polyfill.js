if (!window.WebKitShadowRoot && window.internals) {
	window.WebKitShadowRoot = function(element) {
		return internals.ensureShadowRoot(element);
	}
}
