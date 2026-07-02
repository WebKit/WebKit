import hashlib

def main(request, response):
  ## Get the query parameter (key) from URL ##
  ## Tests will record POST requests (CSP Report) and GET (rest) ##
  if request.GET:
    key = request.GET[b'file']
  elif request.POST:
    key = request.POST[b'file']

  ## Convert the key from String to UUID valid String ##
  testId = hashlib.md5(key).hexdigest()

  ## Handle the header retrieval request ##
  if b'retrieve' in request.GET:
    response.writer.write_status(200)
    response.headers.set(b"Access-Control-Allow-Origin", b"*")
    response.headers.set(b"Cache-Control", b"no-cache, no-store, must-revalidate")
    response.headers.set(b"Pragma", b"no-cache")
    response.headers.set(b"Expires", b"0")
    response.writer.end_headers()
    try:
      value = request.server.stash.take(testId)
      response.writer.write(value)
    except (KeyError, ValueError) as e:
      response.headers.set(b"Access-Control-Allow-Origin", b"*")
      response.headers.set(b"Cache-Control", b"no-cache, no-store, must-revalidate")
      response.headers.set(b"Pragma", b"no-cache")
      response.headers.set(b"Expires", b"0")
      response.writer.end_headers()
      response.writer.write("No report has been recorded " + str(e))
      pass

    response.close_connection = True
    return

  request.server.stash.put(testId, request.body)
  response.headers.set(b"Access-Control-Allow-Origin", b"*")
  response.headers.set(b"Cache-Control", b"no-cache, no-store, must-revalidate")
  response.headers.set(b"Pragma", b"no-cache")
  response.headers.set(b"Expires", b"0")
  response.writer.end_headers()

