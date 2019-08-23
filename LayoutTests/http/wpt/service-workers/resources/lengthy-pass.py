import time

def main(request, response):
    delay = 0.05
    response.headers.set("Content-type", "text/javascript")
    response.write_status_headers()
    time.sleep(delay);
    response.writer.write_content("document")
    time.sleep(delay)
    response.writer.write_content(".body")
    time.sleep(delay)
    response.writer.write_content(".innerHTML = ")
    time.sleep(delay)
    response.writer.write_content("'PASS'")
    time.sleep(delay)
