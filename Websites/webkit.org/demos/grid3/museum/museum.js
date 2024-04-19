// When button with class "hide-controls" is clicked, add "hidden" class 
document.querySelectorAll('.hide-controls').forEach(HTMLElement => {
	HTMLElement.addEventListener('click', (e) => {
		HTMLElement.classList.toggle('hidden');
	});
});

// When button with class "hide-controls" is clicked, 
// change the theme-color metatag to darker blue 
document.querySelector('.hide-controls').addEventListener('click', (e) => {
	document.querySelector('meta[name="theme-color"]').setAttribute('content', 'color(display-p3 0.016 0.166 0.29)')
});
