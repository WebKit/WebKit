import SimpleHTTPServer
import SocketServer
import argparse
import logging
from math import sqrt
from operator import mul
import os
from subprocess import call, check_output
import sys
import threading
import time



# Supress logs from feedback server
logging.getLogger().setLevel(logging.FATAL)


class DefaultLaunchTimeHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def get_test_page(self):
        return '''<!DOCTYPE html>
        <html>
          <head>
            <title>Launch Time Benchmark</title>
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
        pass

    def do_HEAD(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        if not self.path.startswith('/blank'):
            self.wfile.write(self.get_test_page())
        self.wfile.close()

    def do_POST(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write('done')
        self.wfile.close()

        data_string = self.rfile.read(int(self.headers['Content-Length']))
        self.on_receive_stop_signal(data_string)

    def log_message(self, format, *args):
        pass


class LaunchTimeBenchmark:
    def __init__(self):
        self._server_ready = threading.Semaphore(0)
        self._server = None
        self._server_thread = None
        self._port = 8080
        self._feedback_port = None
        self._feedback_server = None
        self._open_count = 0
        self._app_name = None
        self._verbose = False
        self._feedback_in_browser = False
        self._do_not_ignore_first_result = False
        self._iterations = 5
        self._browser_bundle_path = '/Applications/Safari.app'
        self.response_handler = None
        self.benchmark_description = None
        self.use_geometric_mean = False
        self.wait_time_high = 1
        self.wait_time_low = 0.1
        self.iteration_groups = 1
        self.initialize()

    def _parse_browser_bundle_path(self, path):
        if not os.path.isdir(path) or not path.endswith('.app'):
            raise argparse.ArgumentTypeError(
                'Invalid app bundle path: "{}"'.format(path))
        return path

    def _parse_args(self):
        self.argument_parser = argparse.ArgumentParser(description=self.benchmark_description)
        self.argument_parser.add_argument('-p', '--path', type=self._parse_browser_bundle_path,
            help='path for browser application bundle (default: {})'.format(self._browser_bundle_path))
        self.argument_parser.add_argument('-n', '--iterations', type=int,
            help='number of iterations of test (default: {})'.format(self._iterations))
        self.argument_parser.add_argument('-v', '--verbose', action='store_true',
            help="print each iteration's time")
        self.argument_parser.add_argument('-f', '--feedback-in-browser', action='store_true',
            help="show benchmark results in browser (default: {})".format(self._feedback_in_browser))
        self.will_parse_arguments()

        args = self.argument_parser.parse_args()
        if args.iterations:
            self._iterations = args.iterations
        if args.path:
            self._browser_bundle_path = args.path
        if args.verbose is not None:
            self._verbose = args.verbose
        if args.feedback_in_browser is not None:
            self._feedback_in_browser = args.feedback_in_browser
        path_len = len(self._browser_bundle_path)
        start_index = self._browser_bundle_path.rfind('/', 0, path_len)
        end_index = self._browser_bundle_path.rfind('.', 0, path_len)
        self._app_name = self._browser_bundle_path[start_index + 1:end_index]
        self.did_parse_arguments(args)

    def _run_server(self):
        self._server_ready.release()
        self._server.serve_forever()

    def _setup_servers(self):
        while True:
            try:
                self._server = SocketServer.TCPServer(
                    ('0.0.0.0', self._port), self.response_handler)
                break
            except:
                self._port += 1
        print 'Running test server at http://localhost:{}'.format(self._port)

        self._server_thread = threading.Thread(target=self._run_server)
        self._server_thread.start()
        self._server_ready.acquire()

        if self._feedback_in_browser:
            from feedback_server import FeedbackServer
            self._feedback_server = FeedbackServer()
            self._feedback_port = self._feedback_server.start()

    def _clean_up(self):
        self._server.shutdown()
        self._server_thread.join()
        if self._feedback_in_browser:
            self._feedback_server.stop()

    def _exit_due_to_exception(self, reason):
        self.log(reason)
        self._clean_up()
        sys.exit(1)

    def _geometric_mean(self, values):
        product = reduce(mul, values)
        return product ** (1.0 / len(values))

    def _standard_deviation(self, results, mean=None):
        if mean is None:
            mean = sum(results) / float(len(results))
        divisor = float(len(results) - 1) if len(results) > 1 else float(len(results))
        variance = sum((x - mean) ** 2 for x in results) / divisor
        return sqrt(variance)

    def _compute_results(self, results):
        if not results:
            self._exit_due_to_exception('No results to compute.\n')
        if len(results) > 1 and not self._do_not_ignore_first_result:
            results = results[1:]
        mean = sum(results) / float(len(results))
        stdev = self._standard_deviation(results, mean)
        return mean, stdev

    def _wait_times(self):
        if self.iteration_groups == 1:
            yield self.wait_time_high
            return
        increment_per_group = float(self.wait_time_high - self.wait_time_low) / (self.iteration_groups - 1)
        for i in range(self.iteration_groups):
            yield self.wait_time_low + increment_per_group * i

    def open_tab(self, blank=False):
        if blank:
            call(['open', '-a', self._browser_bundle_path,
                'http://localhost:{}/blank/{}'.format(self._port, self._open_count)])
        else:
            call(['open', '-a', self._browser_bundle_path,
                'http://localhost:{}/{}'.format(self._port, self._open_count)])
        self._open_count += 1

    def launch_browser(self):
        if self._feedback_in_browser:
            call(['open', '-a', self._browser_bundle_path,
                'http://localhost:{}'.format(self._feedback_port), '-F'])
            self._feedback_server.wait_until_client_has_loaded()
        else:
            call(['open', '-a', self._browser_bundle_path,
                'http://localhost:{}/blank'.format(self._port), '-F'])
        self.wait(2)

    def quit_browser(self):
        def quit_app():
            call(['osascript', '-e', 'quit app "{}"'.format(self._browser_bundle_path)])

        def is_app_closed():
            out = check_output(['osascript', '-e', 'tell application "System Events"',
                '-e', 'copy (get name of every process whose name is "{}") to stdout'.format(self._app_name),
                '-e', 'end tell'])
            return len(out.strip()) == 0

        while not is_app_closed():
            quit_app()
        self.wait(1)

    def close_tab(self):
        call(['osascript', '-e',
            'tell application "System Events" to keystroke "w" using command down'])

    def wait(self, duration):
        wait_start = time.time()
        while time.time() - wait_start < duration:
            pass

    def log(self, message):
        if self._feedback_in_browser:
            self._feedback_server.send_message(message)
        sys.stdout.write(message)
        sys.stdout.flush()

    def log_verbose(self, message):
        if self._verbose:
            self.log(message)

    def run(self):
        self._parse_args()
        self._setup_servers()
        self.quit_browser()
        print ''

        try:
            group_means = []
            results_by_iteration_number = [[] for _ in range(self._iterations)]

            group = 1
            for wait_duration in self._wait_times():
                self.group_init()
                if self.iteration_groups > 1:
                    self.log('Running group {}{}'.format(group, ':\n' if self._verbose else '...'))

                results = []
                for i in range(self._iterations):
                    try:
                        if not self._verbose:
                            self.log('.')
                        result_in_ms = self.run_iteration()
                        self.log_verbose('({}) {} ms\n'.format(i + 1, result_in_ms))
                        self.wait(wait_duration)
                        results.append(result_in_ms)
                        results_by_iteration_number[i].append(result_in_ms)
                    except KeyboardInterrupt:
                        raise KeyboardInterrupt
                    except Exception as error:
                        self._exit_due_to_exception('(Test {} failed) {}: {}\n'.format(i + 1 if self._verbose else i, type(error).__name__, error))
                if not self._verbose:
                    print ''

                mean, stdev = self._compute_results(results)
                self.log_verbose('RESULTS:\n')
                self.log_verbose('mean: {} ms\n'.format(mean))
                self.log_verbose('std dev: {} ms ({}%)\n\n'.format(stdev, (stdev / mean) * 100))
                if self._verbose:
                    self.wait(1)
                group_means.append(mean)
                group += 1
                self.quit_browser()

            if not self._verbose:
                print '\n'

            if self._feedback_in_browser:
                self.launch_browser()

            means_by_iteration_number = []
            if len(results_by_iteration_number) > 1 and not self._do_not_ignore_first_result:
                results_by_iteration_number = results_by_iteration_number[1:]
            for iteration_results in results_by_iteration_number:
                means_by_iteration_number.append(self._geometric_mean(iteration_results))
            final_mean = self._geometric_mean(group_means)
            final_stdev = self._standard_deviation(means_by_iteration_number)
            self.log('FINAL RESULTS\n')
            self.log('Mean:\n-> {} ms\n'.format(final_mean))
            self.log('Standard Deviation:\n-> {} ms ({}%)\n'.format(final_stdev, (final_stdev / final_mean) * 100))
        except KeyboardInterrupt:
            self._clean_up()
            sys.exit(1)
        finally:
            self._clean_up()

    def group_init(self):
        pass

    def run_iteration(self):
        pass

    def initialize(self):
        pass

    def will_parse_arguments(self):
        pass

    def did_parse_arguments(self, args):
        pass
