import time

def main(request, response):
    headers = [("Content-Type", "text/plain")]
    headers.append(("Access-Control-Allow-Origin", "*"))
    headers.append(("Access-Control-Allow-Methods", "GET"))
    headers.append(("Access-Control-Allow-Headers", "header"))

    if request.method == "OPTIONS":
        return 200, headers, ""

    for header in headers:
        response.headers.set(header[0], header[1])
    response.write_status_headers()
    response.writer.write_content("START\n")
    time.sleep(5);
    response.writer.write_content("END\n")
