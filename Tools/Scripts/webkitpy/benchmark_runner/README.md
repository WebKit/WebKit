# Benchmark Runner 
This is a script for automating the browser based benchmarks(e.g. Speedometer, JetStream)
## Project Structure
```
benchmark_runner
├── README.md
├── __init__.py
├── benchmark_builder.py
├── benchmark_results.py
├── benchmark_results_unittest.py
├── benchmark_runner.py
├── browser_driver
│   ├── __init__.py
│   ├── browser_driver.py
│   ├── browser_driver_factory.py
│   ├── osx_browser_driver.py
│   ├── osx_chrome_driver.py
│   ├── osx_firefox_driver.py
│   └── osx_safari_driver.py
├── data
│   ├── patches
│   │   ├── Dromaeo.patch
│   │   ├── JSBench.patch
│   │   ├── JetStream.patch
│   │   ├── Kraken.patch
│   │   ├── Octane.patch
│   │   ├── Speedometer.patch
│   │   └── SunSpider.patch
│   └── plans
│       ├── dromaeo-cssquery.plan
│       ├── dromaeo-dom.plan
│       ├── dromaeo-jslib.plan
│       ├── jetstream.plan
│       ├── jsbench.plan
│       ├── kraken.plan
│       ├── octane.plan
│       ├── speedometer.plan
│       └── sunspider.plan
├── generic_factory.py
├── http_server_driver
│   ├── __init__.py
│   ├── http_server
│   │   └── twisted_http_server.py
│   ├── http_server_driver.py
│   ├── http_server_driver_factory.py
│   └── simple_http_server_driver.py
├── run_benchmark.py
└── utils.py
```
## Requirements
* python modules:
    * PyObjc
    * psutils (optional)

## HOW TOs
### How to run
```shell
    python path/to/run-benchmark --build-directory path/to/browser/directory --plan json_format_plan --platform target_platform --browser target_browser
```
* **path/to/browser/directory**: should be the folder containing the executable binary(e.g. /Application/ on OSX which contains Safari.app)
* **json_format_plan**: the benchmark plan you want to execute  

### How to create a plan
To create a plan, you may refer to Plans/jetstream.plan.
```json 
{
    "timeout" : 600,
    "count": 5,
    "local_copy": "../../../../PerformanceTests/JetStream",
    "benchmark_patch": "data/patches/JetStream.patch",
    "entry_point": "JetStream/JetStream-1.0.1/index.html",
    "config": {
        "orientation": "portrait"
    },
    "output_file": "jetstream.result"
}
```
Plan is a json-formatted dictionary which contains following keys 
* **timeout**: time limit for **EACH RUN** of the benchmark. This can avoid program getting stuck in the extreme circumstances. The time limit is suggested to be 1.5-2x the time spent in a normal run.
* **count**: the number of times you want to run benchmark
* **local_copy**: (**OPTIONAL**) Path of benchmark, a relative path to the root of this project ('benchmark_runner' directory)
* **remote_archive**: (**OPTIONAL**) URL of the remote (http/https) ZIP file that contains the benchmark.
* **benchmark_path**: (**OPTIONAL**) path of patch, a relative path to the root of this project ('benchmark_runner' directory)
* **entry_point**: the relative url you want browser to launch (a relative path to the benchmark directory)
* **config**: a dictionary that specifies the environment configurations for the test (e.g. orientation while the test is running)
* **output_file**: specify the output file, this can be overwritten by specifying '--output-file' while invoking run-benchmark script

### How to import a benchmark
* Modify the benchmark html file, make sure the page has following functionalities:
    * When you launch the page('entry_point' in benchmark), the test will start automatically
    * Organizing the results
    * 'POST' information to '/report', and 'GET' '/shutdown' when post succeeds. Example:
    * ```js
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "/report");
      xhr.setRequestHeader("Content-type", "application/json");
      xhr.setRequestHeader("Content-length", results.length);
      xhr.setRequestHeader("Connection", "close");
      xhr.onreadystatechange = function() {
          if(xhr.readyState == 4 && xhr.status == 200) {
              var closeRequest = new XMLHttpRequest();
              closeRequest.open("GET", '/shutdown');
              closeRequest.send()
          }
      }
      xhr.send(results);
``` 
* Create a patch file against original file
    * Go to the directory contains the benchmark directory (e.g. for JetStream, you should go to PerformaceTest folder)
    * Use ```git diff --relative HEAD > your.patch``` to create your patch
    * (**Suggested**) move the patch to the 'Patches' directory under project directory
* Create a plan for the benchmark (refer to **"How to create a plan"** for more details)
* Do following instruction **ONLY IF NEEDED**. In most case, you do not have to.
    * If you want to customize BrowserDriver for specific browser/platform, you need to extend browser_driver/browser_driver.py and register your module in browser_driver/browser_driversjson.
    * If you want to customize HTTPServerDriver, you need to extend http_server_drirver/http_server_driver and register your module in http_server_driver/http_server_drivers.json.
