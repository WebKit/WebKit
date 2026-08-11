// Helpers for testing AXTextMarkers

// Walks every document text-marker index spanned by `container` and measures how far the two
// invariants deviate:
//   * round-trip:      indexForTextMarker(textMarkerForIndex(i)) should equal i.
//   * string-length:   the string from the container start to textMarkerForIndex(i) should be
//                      (i - containerStartIndex) characters long.
// Returns the worst (maximum absolute) deviation of each as { worstRoundTripDrift, worstLengthDifference },
// plus indicesChecked (how many index positions were walked) and containerTextLength (the container's
// text length, measured independently). Callers should assert indicesChecked matches containerTextLength
// so a collapsed/degenerate index space can't pass vacuously by making the walk iterate zero times.
//
// Callers should scope this to a container (rather than the web area) so that DOM injected by
// js-test.js — its description and console elements, added at the top of the body — is not part of
// the tree under test. AXIndexForTextMarker / AXTextMarkerForIndex are document-relative, so this
// walks the range of document indices the container spans and offsets lengths by the container's start.
//
// Bounded, non-accumulating deviations are expected (e.g. snapping at a newline or a table cell
// boundary); the bug these guard against is drift that accumulates down the page, so callers should
// assert the returned values stay within a small tolerance that does not grow with content length.
function checkRoundTripAllTextMarkerIndicesWithinContainer(container) {
    var containerRange = container.textMarkerRangeForElement(container);
    var startMarker = container.startTextMarkerForTextMarkerRange(containerRange);
    var endMarker = container.endTextMarkerForTextMarkerRange(containerRange);
    var baseIndex = container.indexForTextMarker(startMarker);
    var lastIndex = container.indexForTextMarker(endMarker);

    var worstRoundTripDrift = 0;
    var worstLengthDifference = 0;
    for (var index = baseIndex; index <= lastIndex; index++) {
        var marker = container.textMarkerForIndex(index);
        worstRoundTripDrift = Math.max(worstRoundTripDrift, Math.abs(container.indexForTextMarker(marker) - index));
        var prefixLength = container.stringForTextMarkerRange(container.textMarkerRangeForMarkers(startMarker, marker)).length;
        worstLengthDifference = Math.max(worstLengthDifference, Math.abs(prefixLength - (index - baseIndex)));
    }
    return {
        worstRoundTripDrift: worstRoundTripDrift,
        worstLengthDifference: worstLengthDifference,
        indicesChecked: lastIndex - baseIndex,
        containerTextLength: container.stringForTextMarkerRange(containerRange).length,
    };
}
