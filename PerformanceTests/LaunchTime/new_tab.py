#! /usr/bin/env python
import argparse
import time
from threading import Event

from launch_time import LaunchTimeBenchmark, DefaultLaunchTimeHandler

class NewTabBenchmark(LaunchTimeBenchmark):
    def _parse_wait_time(self, string):
        values = string.split(':')

        start = None
        end = None
        try:
            if len(values) == 2:
                start = float(values[0])
                end = float(values[1])
                if start > end:
                    raise
            elif len(values) == 1:
                start = float(values[0])
                end = start
            else:
                raise
        except:
            raise argparse.ArgumentTypeError(
                "'" + string + "' is not a range of numbers. Expected form is N:M where N < M")

        return start, end

    def initialize(self):
        self.benchmark_description = "Measure time to open a new tab for a given browser."
        self.response_handler = NewTabBenchmark.ResponseHandler(self)
        self.start_time = None
        self.stop_time = None
        self.stop_signal_was_received = Event()
        self.allow_prewarm = True

    def run_iteration(self):
        tabs_to_open = 1 if self.allow_prewarm else 2
        self.stop_time = None
        for _ in range(tabs_to_open - 1):
            self.open_tab(blank=True)
        self.start_time = time.time() * 1000
        self.open_tab()
        while self.stop_time is None:
            self.stop_signal_was_received.wait()
        result = self.stop_time - self.start_time
        self.stop_signal_was_received.clear()
        for _ in range(tabs_to_open):
            self.close_tab()

        return result

    def group_init(self):
        self.launch_browser()

    def get_test_name(self):
        return "NewTabBenchmark"

    def will_parse_arguments(self):
        self.argument_parser.add_argument('-g', '--groups', type=int,
            help='number of groups of iterations to run (default: {})'.format(self.iteration_groups))
        self.argument_parser.add_argument('-w', '--wait-time', type=self._parse_wait_time,
            help='wait time to use between iterations or range to scan (format is "N" or "N:M" where N < M, default: {}:{})'.format(self.wait_time_low, self.wait_time_high))
        self.argument_parser.add_argument('--no-prewarm', action='store_true',
            help='attempt to ignore process prewarming (will most likely raise standard deviation)')

    def did_parse_arguments(self, args):
        if args.groups:
            self.iteration_groups = args.groups
        if args.wait_time:
            self.wait_time_low, self.wait_time_high = args.wait_time
        if args.no_prewarm:
            self.allow_prewarm = False

    @staticmethod
    def ResponseHandler(new_tab_benchmark):
        class Handler(DefaultLaunchTimeHandler):
            def get_test_page(self):
                return '''<!DOCTYPE html>
                <html>
                  <head>
                    <title>New Tab Benchmark</title>
                    <meta http-equiv="Content-Type" content="text/html" />
                    <script>
                        function sendDone() {
                            const time = performance.timing.navigationStart
                            const request = new XMLHttpRequest();
                            request.open("POST", "done", false);
                            request.setRequestHeader('Content-Type', 'application/json');
                            request.send(JSON.stringify(time));
                        }
                        window.onload = sendDone;
                    </script>
                  </head>
                  <body>
                    <h1>New Tab Benchmark</h1>
                  </body>
                </html>
                '''

            def on_receive_stop_signal(self, data):
                new_tab_benchmark.stop_time = float(data)
                new_tab_benchmark.stop_signal_was_received.set()

        return Handler


if __name__ == '__main__':
    NewTabBenchmark().run()
