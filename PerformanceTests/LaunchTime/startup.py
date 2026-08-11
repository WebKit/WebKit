#! /usr/bin/env python
import time
from threading import Event

from launch_time import LaunchTimeBenchmark, DefaultLaunchTimeHandler


class StartupBenchmark(LaunchTimeBenchmark):
    def initialize(self):
        self.benchmark_description = 'Measure startup time of a given browser.'
        self.response_handler = StartupBenchmark.ResponseHandler(self)
        self.start_time = None
        self.stop_time = None
        self.stop_signal_was_received = Event()

    def run_iteration(self):
        self.start_time = time.time() * 1000
        self.open_tab()
        while self.stop_time is None:
            self.stop_signal_was_received.wait()
        result = self.stop_time - self.start_time
        self.stop_time = None
        self.stop_signal_was_received.clear()
        self.quit_browser()
        return result

    def get_test_name(self):
        return "StartupBenchmark"

    @staticmethod
    def ResponseHandler(startup_benchmark):
        class Handler(DefaultLaunchTimeHandler):
            def get_test_page(self):
                return '''<!DOCTYPE html>
                <html>
                  <head>
                    <title>Startup Benchmark</title>
                    <meta http-equiv="Content-Type" content="text/html" />
                    <script>
                        function sendDone() {
                            const time = Date.now();
                            const request = new XMLHttpRequest();
                            request.open("POST", "done", false);
                            request.setRequestHeader('Content-Type', 'application/json');
                            request.send(JSON.stringify(time));
                        }
                        window.onload = sendDone;
                    </script>
                  </head>
                  <body>
                    <h1>Startup Benchmark</h1>
                  </body>
                </html>
                '''

            def on_receive_stop_signal(self, data):
                startup_benchmark.stop_time = float(data)
                startup_benchmark.stop_signal_was_received.set()

        return Handler


if __name__ == '__main__':
    StartupBenchmark().run()
