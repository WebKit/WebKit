def main(request, response):
    headers = [("Cache-Control", "no-cache"),
               ("Pragma", "no-cache"),
               ("Location", request.GET['location'])]
    return 302, headers, ""
