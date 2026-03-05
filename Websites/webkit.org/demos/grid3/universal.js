// When button with class "hide-controls" is clicked, add "hidden" class 
document.querySelectorAll('.hide-controls').forEach(element => {
    element.addEventListener('click', () => {
        element.classList.toggle('hidden');
    });
});

// When button with class "hide-controls" is clicked, 
// change the theme-color metatag to #FFF white 
document.querySelector('.hide-controls').addEventListener('click', () => {
    document.querySelector('meta[name="theme-color"]').setAttribute('content', '#FFF');
});