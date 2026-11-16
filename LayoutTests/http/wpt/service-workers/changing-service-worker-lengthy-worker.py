import random

def main(request, response):
    response.headers.set(b"Content-type", b"text/javascript")
    response.headers.append(b"Cache-Control", b"no-store")
    response.write_status_headers()

    response.writer.write_content("//" + str(random.random()) + "\n")
    response.writer.write_content("function doFetch(event)\n")
    response.writer.write_content("{\n")
    response.writer.write_content("    event.respondWith(fetch('/WebKit/service-workers/resources/lengthy-pass.py?delay=0.5'));\n")
    response.writer.write_content("}\n")
    response.writer.write_content("self.addEventListener('fetch', doFetch);\n")

    response.writer.write_content("function doMessage(event)\n")
    response.writer.write_content("{\n")
    response.writer.write_content("    event.source.postMessage(!!self.didActivateEventFired);\n")
    response.writer.write_content("}\n")
    response.writer.write_content("self.addEventListener('message', doMessage);\n")

    response.writer.write_content("function doActivate(event)\n")
    response.writer.write_content("{\n")
    response.writer.write_content("    self.didActivateEventFired = true;\n")
    response.writer.write_content("}\n")
    response.writer.write_content("self.addEventListener('activate', doActivate);")
