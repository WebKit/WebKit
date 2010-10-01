def web_socket_do_extra_handshake(request):
  pass

def web_socket_transfer_data(request):
  msg = "\0hello\xff"
  msg += "\x80\x81\x01"   # skip 1*128+1 bytes.
  msg += "\x01"
  msg += "\0should be skipped" + (" " * 109) + "\xff"
  msg += "\0world\xff"
  request.connection.write(msg)
  print msg
