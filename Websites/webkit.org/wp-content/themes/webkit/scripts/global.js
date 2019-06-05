document.addEventListener('DOMContentLoaded', function () {

    function enableScrollableTables () {
        let tables = document.querySelectorAll('#content .bodycopy > table');

        for (let table of tables) {
            let scrollableDiv = document.createElement('div');
            let paddingDiv = document.createElement('div');

            scrollableDiv.classList.add('scrollable');
            paddingDiv.classList.add('scrollable-padding');

            scrollableDiv.appendChild(paddingDiv);
            table.parentNode.insertBefore(scrollableDiv, table);

            paddingDiv.appendChild(table);
        }
    }

    function lazyLoadImages () {
        let backgroundImages = document.querySelectorAll('#content div[data-url]');
        if (backgroundImages.length == 0)
            return;
        let lazyloadImageObserver = new IntersectionObserver(function (changes) {
            for (let change of changes) {
                if (!change.isIntersecting)
                    return;
                change.target.style.backgroundImage = 'url(' + change.target.getAttribute('data-url') + ')';
                change.target.parentElement.classList.add('loaded');
                lazyloadImageObserver.unobserve(change.target);
            }
        });
        for (let element of backgroundImages)
            lazyloadImageObserver.observe(element);
    }

    if (window.IntersectionObserver)
        lazyLoadImages();
    enableScrollableTables();

});
