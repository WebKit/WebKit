
let imageLoader = undefined;

class ImageLoader {
    constructor()
    {
        this.setupObserver();
        this.findImages();
    }
    
    findImages()
    {
        let images = document.getElementsByTagName('img');
        for (let image of images) {
            image.src = this.smallURLForImage(image);
            this.observer.observe(image);
        }
    }
    
    smallURLForImage(image)
    {
        let imageID = image.getAttribute('data-id');
        let smallSize = image.getAttribute('data-size-small');
        return `https://picsum.photos/${smallSize}?image=${imageID}`;
    }

    largeURLForImage(image)
    {
        let imageID = image.getAttribute('data-id');
        let largeSize = image.getAttribute('data-size-large');
        return `https://picsum.photos/${largeSize}?image=${imageID}`;
    }

    setupObserver()
    {
        let options = { 
            rootMargin: "0px 0px" 
        };
        this.observer = new IntersectionObserver(observations => {
            this.intersectionsChanged(observations);
        }, options);
    }
    
    intersectionsChanged(observations)
    {
        for (let obs of observations) {
            let image = obs.target;
            // If we haven't done it yet
            if (obs.isIntersecting) {
                // timeout just for demo
                setTimeout(() => {
                    image.src = this.largeURLForImage(image);
                }, 500);
            }
        }
    }
};

window.addEventListener('load', () => {
    imageLoader = new ImageLoader();
}, false);