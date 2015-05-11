# Benchmark Runner 
This is a script for automating the browser based benchmarks(e.g. Speedometer, JetStream)
## Project Structure
```
benchmark_runner
├── README.md
├── __init__.py
├── benchmark_builder
│   ├── __init__.py
│   ├── benchmark_builder_factory.py
│   ├── benchmark_builders.json
│   ├── generic_benchmark_builder.py
│   └── jetstream_benchmark_builder.py
├── benchmark_runner.py
├── browser_driver
│   ├── __init__.py
│   ├── browser_driver.py
│   ├── browser_driver_factory.py
│   ├── browser_drivers.json
│   ├── osx_chrome_driver.py
│   └── osx_safari_driver.py
├── data
│   ├── patches
│   │   ├── JetStream.patch
│   │   └── Speedometer.patch
│   └── plans
│       ├── jetstream.plan
│       └── speedometer.plan
├── generic_factory.py
├── http_server_driver
│   ├── __init__.py
│   ├── http_server
│   │   └── twisted_http_server.py
│   ├── http_server_driver.py
│   ├── http_server_driver_factory.py
│   ├── http_server_drivers.json
│   └── simple_http_server_driver.py
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
    "http_server_driver": "SimpleHTTPServerDriver", 
    "timeout" : 600,
    "count": 5,
    "benchmark_builder": "JetStreamBenchmarkBuilder",
    "local_copy": "../../../../PerformanceTests/JetStream",
    "benchmark_patch": "data/patches/JetStream.patch",
    "entry_point": "JetStream/JetStream-1.0.1/index.html",
    "output_file": "jetstream.result"
}
```
Plan is a json-formatted dictionary which contains following keys 
* **http_server_driver**: (**case-sensitive**) the http server module you want to host the resources. Current available option is "SimpleHTTPServerHandle" which is based on python twisted framework.
* **timeout**: time limit for **EACH RUN** of the benchmark. This can avoid program getting stuck in the extreme circumstances. The time limit is suggested to be 1.5-2x the time spent in a normal run.
* **count**: the number of times you want to run benchmark
* **benchmark_builder**:  builder of the benchmark which is responsible for arranging benchmark before the web server serving the directory. In most case, 'GenericBenchmarkHandler' is sufficient. It copies the benchmark to a temporary directory and applies patch to benchmark. If you have special requirement, you could design your own benchmark handle, just like the 'JetStreamBenchmarkHandle' in this example.
* **local_copy**: path of benchmark, a relative path to the root of this project ('benchmark_runner' directory)
* **remote_archive**: (**OPTIONAL**) URL of the remote (http/https) ZIP file that contains the benchmark.
* **benchmark_path**: (**OPTIONAL**) path of patch, a relative path to the root of this project ('benchmark_runner' directory)
* **entry_point**: the relative url you want browser to launch (a relative path to the benchmark directory)
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
    * If you want to customize BenchmarkBuilder, you need to extend benchmark_builder/generic_benchmark_builder register you module in benchmark_builder/benchmark_builders.json
