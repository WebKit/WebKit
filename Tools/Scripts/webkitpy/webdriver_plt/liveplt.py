from pltresults import PLTResults
from selenium import webdriver
from time import sleep
from webkitpy.benchmark_runner.utils import get_driver_binary_path
from webkitpy.common.timeout_context import Timeout

# FIXME: we want to avoid hardcoding the offsets
# we use this to make sure the window.innerHeight of each browser is the same
browser_height_offsets = {
    'safari': 76,
    'chrome': 0,
    'firefox': 37,
    'stp': 76,
}


class PageLoadTest(object):

    def __init__(self, iterations, instances, wait, browser, suites, size):
        print("Creating PLT with parameters:")
        print("iterations: {}".format(iterations))
        print("instances: {}".format(instances))
        print("wait: {}s".format(wait))
        print("browser: {}".format(browser))
        print("suites: {}".format([s.name for s in suites]))
        width, height = size
        if width:
            print("width: {}".format(width))
        if height:
            print("height: {}".format(height))
        print("")
        self.iterations = iterations
        self.instances = instances
        self.wait = wait
        self.browser = browser
        self.suites = suites
        self.size = size

    def start(self):
        total_results = PLTResults()
        suiteid = 0
        for suite in self.suites:
            suite.attempts -= 1
            suiteid += 1
            print("------------------------------")
            print("Running suite {id} of {num}: {name}".format(id=suiteid, num=len(self.suites), name=suite.name))
            print("------------------------------")
            print("")
            total_results += self.run_suite(suite)
        total_results.print_results()

    def run_suite(self, suite):
        cold_run_results = PLTResults()
        suite_results = PLTResults()

        for instance in range(self.instances):
            driver = self._get_driver_for_browser(self.browser)
            self._setup_browser_window(driver)
            sleep(5)
            for iteration in range(self.iterations + 1):
                print("------------------------------")
                if iteration == 0:
                    print("Running iternation %s (cold run)" % iteration)
                else:
                    print("Running iternation %s" % iteration)
                run_results = self.run_one_test(suite, driver)
                if not run_results:
                    if suite.attempts <= 0:
                        print("Failed to finish suite {name} after {x} attempts. Results are incomplete.".format(name=suite.name, x=suite.max_attempts))
                        print("Exiting...")
                        quit()
                    else:
                        print("A page failed to load. Re-queuing suite.")
                        self.suites.append(suite)
                        driver.quit()
                        return PLTResults()
                run_results.print_url_results("INDIVIDUAL")
                if iteration == 0:
                    cold_run_results += run_results
                else:
                    suite_results += run_results
            driver.quit()
        cold_run_results.print_results(suite.name, True)
        suite_results.print_results(suite.name)
        return suite_results

    def _get_driver_for_browser(self, browser):
        driver_executable = get_driver_binary_path(browser)
        if browser == 'safari':
            return webdriver.Safari()
        if browser == 'chrome':
            from selenium.webdriver.chrome.options import Options
            options = Options()
            options.add_argument("--disable-web-security")
            options.add_argument("--disable-extensions")
            options.add_argument("--start-maximized")
            return webdriver.Chrome(chrome_options=options, executable_path=driver_executable)
        if browser == 'firefox':
            return webdriver.Firefox(executable_path=driver_executable)
        if browser == 'stp':
            return webdriver.Safari(executable_path='/Applications/Safari Technology Preview.app/Contents/MacOS/safaridriver')

    def _setup_browser_window(self, driver):
        driver.maximize_window()
        sleep(1)
        current_size = driver.get_window_size()

        new_width, new_height = self.size
        if not new_width:
            new_width = driver.execute_script('return screen.width;')
            print("setting width to {}".format(new_width))
        if not new_height:
            new_height = current_size['height'] - browser_height_offsets[self.browser]
            print("setting height to {}".format(new_height))

        try:
            driver.set_window_size(width=new_width, height=new_height)
            driver.set_window_position(x=0, y=0)
        except:
            pass

    def run_one_test(self, suite, driver):
        tabs = False
        currentTabIdx = 0
        maxTabs = 5
        maxRetrys = 5
        timeout = 30

        local_results = PLTResults()

        for url in suite.urls:
            if tabs:
                if currentTabIdx < maxTabs - 1:
                    raise NotImplementedError("Opening new tabs is not supported yet.")
                driver.switch_to_window(driver.window_handles[currentTabIdx % maxTabs])
                currentTabIdx += 1
            tempWait = self.wait
            for attempt in range(maxRetrys):
                driver.set_page_load_timeout(timeout)
                try:
                    driver.get(url)
                except:
                    print("{url} timed out after {time} seconds.".format(url=url, time=timeout))
                    return None

                sleep(tempWait)
                if self.get_results(driver, local_results, url):
                    break

                print("Could not get results for {url}. Retrying...".format(url=url))
                tempWait += 0.5
                if attempt == maxRetrys - 1:
                    return None
        return local_results

    def get_results(self, driver, results, url):
        nt_results = driver.execute_script('return performance.getEntriesByType("navigation")[0]')  # navigation timing 2
        if not nt_results:
            nt_results = driver.execute_script('return performance.timing')  # navigation timing 1
            start = nt_results['navigationStart']
            end = nt_results['loadEventEnd']
            if start == 0 or end == 0:
                return False
        else:
            start = nt_results['startTime']
            end = nt_results['loadEventEnd']
            if end == 0:
                return False

        results.add_timing_result(end - start, url)
        return True
