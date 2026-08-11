def main(request, response):
    response.status = (200, "OK")
    response.headers.set(b"Content-Type", b'text/javascript')
    response.headers.set(b"Cache-Control", b"max-age=0");

    if b"useCORS" in request.GET:
        response.headers.set(b"Access-Control-Allow-Origin", b"*")

    return """
class DummyProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
  }

  process(inputs, outputs) {
    return false;
  }
}

registerProcessor('dummy-processor', DummyProcessor);"""
