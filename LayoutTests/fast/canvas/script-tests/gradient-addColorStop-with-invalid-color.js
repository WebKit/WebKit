description(
'This test checks invalid colors on gradients.'
);

var gradient = document.createElement('canvas').getContext('2d').createLinearGradient(0, 0, 150, 0);

shouldThrow("gradient.addColorStop(0, '')", "'SyntaxError (DOM Exception 12): The string did not match the expected pattern.'");
shouldThrow("gradient.addColorStop(0, '#cc')", "'SyntaxError (DOM Exception 12): The string did not match the expected pattern.'");
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0)')", "'SyntaxError (DOM Exception 12): The string did not match the expected pattern.'");
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0, 5, 0)')", "'SyntaxError (DOM Exception 12): The string did not match the expected pattern.'");
