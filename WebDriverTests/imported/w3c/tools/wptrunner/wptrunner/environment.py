# mypy: allow-untyped-defs

import errno
import os
import socket
import time

def wait_for_service(logger, host, port, timeout=60):
    """Waits until network service given as a tuple of (host, port) becomes
    available or the `timeout` duration is reached, at which point
    ``socket.error`` is raised."""
    addr = (host, port)
    logger.debug(f"Trying to connect to {host}:{port}")
    end = time.time() + timeout
    while end > time.time():
        so = socket.socket()
        try:
            so.connect(addr)
        except socket.timeout:
            pass
        except OSError as e:
            if e.errno != errno.ECONNREFUSED:
                raise
        else:
            logger.debug(f"Connected to {host}:{port}")
            return True
        finally:
            so.close()
        time.sleep(0.5)
    raise OSError("Service is unavailable: %s:%i" % addr)
