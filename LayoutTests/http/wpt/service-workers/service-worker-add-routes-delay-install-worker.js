let result;
async function addRoute(e, counter)
{
   try {
     await e.addRoute([{
       condition: {urlPattern: new URLPattern({pathname: 'direct.txt' + counter})},
       source: 'network'
     }]);
  } catch (exception) {
    result = "KO:" + exception;
  }

  if (counter >= 256) {
    result = "OK";
    return;
  }

  addRoute(e, ++counter);
}

onfetch = e => { };
onmessage = e => e.source.postMessage(result);
oninstall = e => addRoute(e, 0);
