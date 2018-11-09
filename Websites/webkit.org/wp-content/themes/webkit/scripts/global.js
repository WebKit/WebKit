document.addEventListener('DOMContentLoaded', function () {
    var openClass = ' open-menu',
        menuClass = 'menu-item-has-children',
        menus = document.querySelectorAll('#site-nav > div > .menu > .menu-item'),
        menuLinks = document.querySelectorAll('#site-nav > div > .menu > .menu-item > a'),
        menuitems = document.querySelectorAll('#site-nav .menu-item-has-children .menu-item > a');

    function findParentMenu (element, className) {
        while ( (element = element.parentElement) && ! element.classList.contains(className) );
        return element;
    }

    for (var i = 0; i < menuLinks.length; ++i) {
        menuLinks[i].addEventListener('focus', function (e) {
            var openMenus = findParentMenu(e.target, 'menu').getElementsByClassName(openClass.trim());
            for (var m = 0; m < openMenus.length; ++m) {
                openMenus[m].className = openMenus[m].className.replace(openClass, "");
            }
        });
    }

    for (var i = 0; i < menuitems.length; ++i) {
        menuitems[i].addEventListener('focus', function (e) {
            var targetMenu = findParentMenu(e.target, menuClass),
                targetMenuClass = null;

            if ( targetMenu != undefined )
                targetMenuClass = targetMenu.className;

            for (var m = 0; m < menus.length; ++m) {
                menus[m].className.replace(openClass, "");
                if (menus[m] == targetMenu) {
                    if (targetMenuClass.indexOf(openClass) == -1) {
                        targetMenu.className += openClass;
                    }
                } else {
                    menus[m].className = menus[m].className.replace(openClass, "");
                }
            }
        });
    }

    var latest = [], updating = false;
    function inView(element) {
        var box = element.getBoundingClientRect();
        return ( (box.top >= 0 && box.left >= 0 && box.top) <= (window.innerHeight || document.documentElement.clientHeight));
    }

    function stageImage(element, src) {
        element.style.backgroundImage = 'url(' + src + ')';
        if (!element.parentElement.classList.contains('loaded'))
            element.parentElement.classList.add('loaded');
    }

    function loadImage(element) {
        var src = element.getAttribute('data-url');

        if (sessionStorage.getItem(src)) {
            setTimeout(function () { stageImage(element, src); }, 1);
        } else {
            var img = new Image();
            img.src = src;
            img.onload = function() {
                stageImage(element,src);

                try {
                    sessionStorage.setItem(src, true);
                } catch (error) {
                    return false; // private browsing
                }
                img = undefined;
            }
        }

    }

    function onMovement() {
        if (!updating)
            requestAnimationFrame(updateImages);
        updating = true;
    }

    function updateImages() {
        updating = false;

        for (var i = 0; i < imgs.length; i++) {
            if ( inView(imgs[i]) )
                loadImage(imgs[i]);
        }

    }

    function enableScrollableTables () {
        var tables = document.querySelectorAll('.bodycopy > table');
        var tableCount = tables.length;

        for (var i = 0; i < tableCount; i++) {
            var scrollableDiv = document.createElement('div');
            var paddingDiv = document.createElement('div');

            scrollableDiv.classList.add('scrollable');
            paddingDiv.classList.add('scrollable-padding');

            scrollableDiv.appendChild(paddingDiv);
            tables[i].parentNode.insertBefore(scrollableDiv, tables[i]);

            paddingDiv.appendChild(tables[i]);
        }
    }

    var imgs = document.querySelectorAll('div[data-url]');
    document.addEventListener('scroll', onMovement);
    document.addEventListener('resize', onMovement);

    updateImages();
    enableScrollableTables();

});
