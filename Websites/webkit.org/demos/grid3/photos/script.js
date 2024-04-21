// For each card, if it's clicked, toggle the `expanded` class (add if it doesn't have, remove if it does)
// And when one card is clicked to be expanded, remove the `expanded` class from any other card that currently has it 
document.querySelectorAll('.card').forEach(element => {
	element.addEventListener('click', (e) => {
		document.startViewTransition(() => {
			const clickedElement = element;
			const currentlyExpanded = document.querySelector('.expanded');
			if (currentlyExpanded && clickedElement != currentlyExpanded) {
				currentlyExpanded.classList.remove('expanded');
			}
			clickedElement.classList.toggle('expanded');
		});
	});
});