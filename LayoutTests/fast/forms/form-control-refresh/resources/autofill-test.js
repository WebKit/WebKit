function test(shouldObscure) {
    const topShield = document.createElement("div");
    const bottomShield = document.createElement("div");
    const leftShield = document.createElement("div");
    const rightShield = document.createElement("div");

    function hideElementBorders(element) {
        const inputBounds = element.getBoundingClientRect();

        topShield.style.left = inputBounds.left + "px";
        topShield.style.top = inputBounds.top - 4 + "px";
        topShield.style.width = inputBounds.width + "px";
        topShield.style.height = "8px";

        bottomShield.style.left = inputBounds.left + "px";
        bottomShield.style.top = inputBounds.top + inputBounds.height - 4 + "px";
        bottomShield.style.width = inputBounds.width + "px";
        bottomShield.style.height = "8px";

        leftShield.style.left = inputBounds.left - 4 + "px";
        leftShield.style.top = inputBounds.top - 4 + "px";
        leftShield.style.width = 8 + "px";
        leftShield.style.height = inputBounds.height + 8 + "px";

        rightShield.style.left = inputBounds.left + inputBounds.width - 4 + "px";
        rightShield.style.top = inputBounds.top - 4 + "px";
        rightShield.style.width = 8 + "px";
        rightShield.style.height = inputBounds.height + 8 + "px";

        topShield.classList.add("shield");
        bottomShield.classList.add("shield");
        leftShield.classList.add("shield");
        rightShield.classList.add("shield");

        document.body.appendChild(topShield);
        document.body.appendChild(bottomShield);
        document.body.appendChild(leftShield);
        document.body.appendChild(rightShield);
    }

    var textfield = document.getElementById('textfield');
    hideElementBorders(textfield);

    const nativeCSSBackground = window.getComputedStyle(textfield).background;

    if (window.internals) {
        if (shouldObscure) {
            window.internals.setAutofilledAndObscured(textfield, true);
            window.internals.setAutofilledAndObscured(devolved, true);
        } else {
            window.internals.setAutofilled(textfield, true);
            window.internals.setAutofilled(devolved, true);
        }

        const nativeCSSBackgroundWithAutoFill = window.getComputedStyle(textfield).background;
        const devolvedCSSBackgroundWithAutoFill = window.getComputedStyle(devolved).background;

        const nativeCSSColorWithAutoFill = window.getComputedStyle(textfield).color;
        const devolvedCSSColorWithAutoFill = window.getComputedStyle(devolved).color;

        if (nativeCSSBackground == nativeCSSBackgroundWithAutoFill)
            message.innerText += "FAIL: The CSS background did not update for the text field after being autofilled.\n";

        if (nativeCSSBackgroundWithAutoFill != devolvedCSSBackgroundWithAutoFill)
            message.innerText += "FAIL: The CSS background differs between the autofilled native control and the autofilled devolved control.\n";

        if (nativeCSSColorWithAutoFill != devolvedCSSColorWithAutoFill)
            message.innerText += "FAIL: The CSS color differs between the autofilled native control and the autofilled devolved control.\n";

        topShield.style.background = devolvedCSSBackgroundWithAutoFill;
        bottomShield.style.background = devolvedCSSBackgroundWithAutoFill;
        leftShield.style.background = devolvedCSSBackgroundWithAutoFill;
        rightShield.style.background = devolvedCSSBackgroundWithAutoFill;
        document.body.style.background = devolvedCSSBackgroundWithAutoFill;
    }    
}
