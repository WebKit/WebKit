oninstall = e => {
   const routes = [];
   for (let i = 0; i < 1024; ++i) {
       routes.push({
           condition: {urlPattern: new URLPattern({pathname: '/**/direct.txt'})},
           source: 'network'
       });
   }
   // FIXME: Remove waitUntil call.
   e.waitUntil(e.addRoutes(routes));
}
