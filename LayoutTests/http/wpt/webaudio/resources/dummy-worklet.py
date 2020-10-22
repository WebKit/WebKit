def main(request, response):
    response.status = (200, "OK")
    response.headers.set("Content-Type", 'text/javascript')
    response.headers.set("Cache-Control", "max-age=0");

    if "useCORS" in request.GET:
        response.headers.set("Access-Control-Allow-Origin", "*")

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
