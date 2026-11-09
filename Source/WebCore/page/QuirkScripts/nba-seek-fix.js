if (!window.__nbaSeekFix) {
    window.__nbaSeekFix = true;
    document.addEventListener('touchmove', function({ target, touches }) {
        if (!target?.getAttribute
            || target.getAttribute('data-id') !== 'video-player:scrub-bar:controls'
            || !touches?.[0])
            return;
        const touch = touches[0];
        const rect = target.getBoundingClientRect();
        const event = new MouseEvent('mousemove', {
            clientX: touch.clientX,
            clientY: touch.clientY,
            screenX: touch.screenX,
            screenY: touch.screenY,
            bubbles: true,
            cancelable: true
        });
        Object.defineProperty(event, 'offsetX', { value: touch.clientX - rect.left, configurable: true });
        Object.defineProperty(event, 'offsetY', { value: touch.clientY - rect.top, configurable: true });
        target.dispatchEvent(event);
    }, false);
}
