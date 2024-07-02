# Rendering Tests

Tests the [rendering section of the WebVTT spec.](https://w3c.github.io/webvtt/#rendering)

WebVTT Rendering Tests are generally reference tests, made up of the expected
output in yellow and the actual rendered output in red, where the test uses 
`[mix-blend-mode](https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode)` 
to combine the two into a green result. A passing test will contain no yellow
or red pixels.