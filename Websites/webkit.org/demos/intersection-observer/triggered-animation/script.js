let animations = undefined;
class AnimationManager {
    constructor()
    {
        this.setupObserver();
        
        let targets = document.querySelectorAll('.animation-container');
        for (let target of targets) {
            this.observer.observe(target);
        }
    }
    
    setupObserver()
    {
        this.observer = new IntersectionObserver((observations) => {
            this.intersectionsChanged(observations);
        });
    }
    
    intersectionsChanged(observations)
    {
        for (let obs of observations) {
            if (obs.isIntersecting)
                obs.target.classList.add('visible');
            else
                obs.target.classList.remove('visible');
        }
    }
};

window.addEventListener('load', () => {
    animations = new AnimationManager();
}, false);
