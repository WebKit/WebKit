from math import sqrt


class URLResults(object):

    def __init__(self, totaltime=0, num_urls=0, std_dev=0.0):
        self.time = totaltime
        self.count = num_urls
        self.std_dev_counter = std_dev

    def __add__(self, other):
        new_time = self.time + other.time
        new_count = self.count + other.count
        new_std_dev_counter = self.std_dev_counter + other.std_dev_counter
        return URLResults(new_time, new_count, new_std_dev_counter)

    @property
    def mean(self):
        if self.count:
            return self.time / float(self.count)
        else:
            return 0

    @property
    def coef_variance(self):
        n = self.count
        if n <= 1:
            return 0
        std_dev = sqrt((self.std_dev_counter / float(n - 1)) - (n / float(n - 1)) * (self.mean ** 2))
        return (std_dev * 100.0) / self.mean

    def print_results(self, url, results_format_str):
        str_time = self._format_time(self.mean)
        print(results_format_str.format(time=str_time.ljust(12), var="{0:.2f}%".format(self.coef_variance).ljust(10), url=url))

    def _format_time(self, time):
        return "{} ms".format(int(time))
