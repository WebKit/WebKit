async function waitForWindowBlur() {
    return new Promise((resolve) => {
        if (!document.hasFocus())
            resolve();

        const handleBlur = () => {
            window.removeEventListener('blur', handleBlur);
            resolve();
        };

        window.addEventListener('blur', handleBlur);
    });
}

async function waitForWindowFocus() {
    return new Promise((resolve) => {
        if (document.hasFocus())
            resolve();

        const handleFocus = () => {
            window.removeEventListener('focus', handleFocus);
            resolve();
        };

        window.addEventListener('focus', handleFocus);
    });
}
