from collections import OrderedDict
from urlresults import URLResults
from math import log, exp


class PLTResults(object):

    def __init__(self, geo_sum=1, totalsum=0, num_urls=0, urls=None):
        self.geo_sum = geo_sum
        self.sum = totalsum
        self.count = num_urls
        if urls is None:
            urls = OrderedDict()
        self.urls = urls

    def __add__(self, other):
        new_geo_sum = self.geo_sum + other.geo_sum
        new_sum = self.sum + other.sum
        new_count = self.count + other.count
        new_urls = OrderedDict(self.urls)
        for (url, results) in other.urls.items():
            if url not in self.urls:
                new_urls[url] = URLResults()
            new_urls[url] += results
        return PLTResults(new_geo_sum, new_sum, new_count, new_urls)

    def add_timing_result(self, time, url):
        self.geo_sum += log(time)
        self.sum += time
        self.count += 1
        if url in self.urls:
            self.urls[url] += URLResults(time, 1, time ** 2)
        else:
            self.urls[url] = URLResults(time, 1, time ** 2)

    @property
    def mean(self):
        if self.count:
            return self.sum / float(self.count)
        else:
            return 0

    @property
    def geometric_mean(self):
        if self.count:
            return exp(self.geo_sum / float(self.count))
        else:
            return 0

    @property
    def mean_coef_variance(self):
        total = 0.0
        for _, results in self.urls.items():
            total += results.coef_variance
        if self.urls and len(self.urls):
            return total / float(len(self.urls))
        else:
            return 0.0

    def print_results(self, suitename=None, cold_run=False):
        print("------------------------------")
        if not suitename:
            print("")
            print("------------------------------")
            print("OVERALL PLT RESULTS SUMMARY:")
        elif cold_run:
            print("Cold Run Results for {name}:".format(name=suitename))
        else:
            print("PLT Results for {name}:".format(name=suitename))
        if suitename:
            self.print_url_results("MEAN")
        else:
            self.print_url_results("TOTAL MEAN")
        print("------------------------------")
        print("TOTAL TIME: {time}".format(time=self._format_time(self.sum)))
        print("TOTAL URLS: {urls}".format(urls=self.count))
        if self.urls and len(self.urls) and self.urls.values()[0].count > 1:
            print("AVERAGE COEFFICIENT OF VARIANCE: {0:.2f}%".format(self.mean_coef_variance))
        print("AVERAGE TIME PER PAGE: {avg}".format(avg=self._format_time(self.mean)))
        print("GEOMETRIC MEAN: {avg}".format(avg=self._format_time(self.geometric_mean)))
        print("------------------------------")
        print("")

    def print_url_results(self, urlResultsType):
        print("------------------------------")
        print("{type} URL LOAD TIMES:".format(type=urlResultsType))
        if not len(self.urls):
            print("")
            return
        if self.urls.values()[0].count > 1:
            results_format_str = "{time}{var}{url}"
        else:
            results_format_str = "{time}{url}"
        print(results_format_str.format(time="time (ms)".ljust(12), var="CoV (%)".ljust(10), url="url"))
        for (url, results) in self.urls.items():
            results.print_results(url, results_format_str)
        print("")

    def _format_time(self, time):
        return "{} ms".format("{0:.2f}".format(float(time)).rstrip('0').rstrip('.'))
