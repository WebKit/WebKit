onmessage = (event) => {
    switch (event.data.action) {
    case "log":
        console.log(event.data.text);
        break;
    case "clear-then-error":
        console.clear();
        console.error(event.data.text);
        break;
    }
};
