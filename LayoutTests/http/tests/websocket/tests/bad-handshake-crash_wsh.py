def web_socket_do_extra_handshake(request):
  msg = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
  msg += "Upgrade: WebSocket\r\n"
  msg += "Connection: Upgrade\r\n"
  msg += "\xa5:\r\n"
  msg += "\r\n"
  request.connection.write(msg)
  print msg

def web_socket_transfer_data(request):
  pass
