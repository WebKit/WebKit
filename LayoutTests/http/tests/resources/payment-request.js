function activateThen(completion)
{
    return new Promise(resolve => {
        var button = document.createElement("button");
        button.style["position"] = "absolute";
        button.onclick = () => {
            document.body.removeChild(button);
            resolve(completion());
        };
        document.body.insertBefore(button, document.body.firstChild);
        UIHelper.activateElement(button);
    });
}

function user_activation_test(func, name)
{
    promise_test(async t => {
        await activateThen(() => func(t));
    }, name);
}
