def web_socket_do_extra_handshake(request):
  pass

def web_socket_transfer_data(request):
  msg = 16 * '\xff'
  request.connection.write(msg)
