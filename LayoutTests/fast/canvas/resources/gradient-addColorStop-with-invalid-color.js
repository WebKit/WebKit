description(
'This test checks invalid colors on gradients.'
);

var gradient = document.createElement('canvas').getContext('2d').createLinearGradient(0, 0, 150, 0);

shouldThrow("gradient.addColorStop(0, '')", "'Error: SYNTAX_ERR: DOM Exception 12'");
shouldThrow("gradient.addColorStop(0, '#cc')", "'Error: SYNTAX_ERR: DOM Exception 12'");
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0)')", "'Error: SYNTAX_ERR: DOM Exception 12'");
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0, 5, 0)')", "'Error: SYNTAX_ERR: DOM Exception 12'");

var successfullyParsed = true;
