from abc import ABCMeta, abstractmethod


class HTTPServerDriver(metaclass=ABCMeta):
    platforms = []

    @abstractmethod
    def serve(self, webRoot):
        pass

    @abstractmethod
    def fetch_result(self):
        pass

    @abstractmethod
    def kill_server(self):
        pass

    @abstractmethod
    def get_return_code(self):
        pass

    @abstractmethod
    def set_device_id(self, device_id):
        pass

    def set_http_log(self, log_path):
        pass
