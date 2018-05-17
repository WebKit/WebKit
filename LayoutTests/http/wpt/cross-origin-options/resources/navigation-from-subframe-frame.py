def main(request, response):
    headers = [("Content-Type", "text/html"),
               ("Cross-Origin-Options", request.GET['value']),]
    return 200, headers, """<!DOCTYPE html>
<html>
<head>
<script src="/common/get-host-info.sub.js"></script>
</head>
<body>
<script>
const RESOURCES_DIR = "/WebKit/cross-origin-options/resources/";
let f = document.createElement("iframe");
f.src = get_host_info().HTTP_REMOTE_ORIGIN + RESOURCES_DIR + "navigate-parent-via-anchor.html?target=%s";
document.body.prepend(f);
</script>
</body>
</html>""" % request.GET['target']
