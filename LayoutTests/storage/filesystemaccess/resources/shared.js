var jsTestIsAsync = true;

function finishTest(error)
{
    if (error) {
        if (error.toString)
            error = error.toString();

        if (typeof error === 'string')
            testFailed(error);
        else
            debug("WARN: cannot print error because it is not a string");
    }

    finishJSTest();
}

async function asyncReadFileAsText(file) 
{
    return new Promise((resolve, reject) => {
        var reader = new FileReader();
        reader.readAsText(file);
        reader.onload = (event) => {
            resolve(event.target.result); 
        }
        reader.onerror = (event) => {
            reject(event.target.error);
        }
    });
}