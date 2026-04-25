from Queue import Queue
import logging
import os
import socket
import threading
import time

# This line makes sure that tornado is installed and in sys.path
import thirdparty.autoinstalled.tornado
import tornado.ioloop
import tornado.web
import tornado.websocket
import tornado.template
from tornado.httpserver import HTTPServer


class FeedbackServer:
    def __init__(self):
        self._client = None
        self._feedback_server_thread = None
        self._port = 9090
        self._application = None
        self._server_is_ready = None
        self._io_loop = None
        self._messages = Queue()
        self._client_loaded = threading.Semaphore(0)

    def _create_app(self):
        return HTTPServer(tornado.web.Application([
            (r'/ws', FeedbackServer.WSHandler(self)),
            (r'/', FeedbackServer.MainHandler(self)),
        ]))

    def _start_server(self):
        self._io_loop = tornado.ioloop.IOLoop()
        self._io_loop.make_current()
        self._application = self._create_app()
        while True:
            try:
                self._application.listen(self._port)
                print 'Running feedback server at http://localhost:{}'.format(self._port)
                break
            except socket.error as err:
                self._port += 1
            except:
                print 'Feedback server failed to start'
                break
        self._server_is_ready.release()
        self._io_loop.start()

    def _send_all_messages(self):
        if self._client:
            while not self._messages.empty():
                message = self._messages.get()
                self._client.write_message(message)

    def start(self):
        self._server_is_ready = threading.Semaphore(0)
        self._feedback_server_thread = threading.Thread(target=self._start_server)
        self._feedback_server_thread.start()
        self._server_is_ready.acquire()
        return self._port

    def stop(self):
        self._client_loaded = threading.Semaphore(0)
        self._application.stop()
        self._io_loop.add_callback(lambda x: x.stop(), self._io_loop)
        self._feedback_server_thread.join()

    def send_message(self, new_message):
        self._messages.put(new_message)
        self._io_loop.add_callback(self._send_all_messages)

    def wait_until_client_has_loaded(self):
        self._client_loaded.acquire()

    @staticmethod
    def MainHandler(feedback_server):
        class Handler(tornado.web.RequestHandler):
            def get(self):
                loader = tornado.template.Loader('.')
                client_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'feedback_client.html')
                self.write(loader.load(client_path).generate(port=feedback_server._port))

        return Handler

    @staticmethod
    def WSHandler(feedback_server):
        class Handler(tornado.websocket.WebSocketHandler):
            def open(self):
                feedback_server._client = self
                feedback_server._client_loaded.release()

            def on_close(self):
                feedback_server._client = None

        return Handler
