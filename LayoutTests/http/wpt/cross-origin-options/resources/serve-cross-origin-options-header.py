def main(request, response):
    headers = [("Content-Type", "text/html"),
               ("Cross-Origin-Options", request.GET['value']),]
    return 200, headers, """TEST
        <iframe name="subframe"></iframe>
        <script>
            window.addEventListener('load', () => {
                const ownerWindow = window.opener ? window.opener : window.top;
                try {
                    ownerWindow.postMessage("READY", "*");
                } catch (e) { }
            });
        </script>
    """
