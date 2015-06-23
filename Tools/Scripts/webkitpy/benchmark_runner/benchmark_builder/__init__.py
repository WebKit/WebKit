# Required for Python to search this directory for module files

# Keep this file free of any code or import statements that could
# cause either an error to occur or a log message to be logged.
# This ensures that calling code can import initialization code from
# webkitpy before any errors or log messages due to code in this file.
# Initialization code can include things like version-checking code and
# logging configuration code.
#
# We do not execute any version-checking code or logging configuration
# code in this file so that callers can opt-in as they want.  This also
# allows different callers to choose different initialization code,
# as necessary.
import os

from webkitpy.benchmark_runner.utils import load_subclasses
from benchmark_builder_factory import BenchmarkBuilderFactory


def benchmark_builder_loader(benchmark_builder_class):
    if benchmark_builder_class.builder_name:
        BenchmarkBuilderFactory.add(benchmark_builder_class.builder_name, benchmark_builder_class)


load_subclasses(
    dirname=os.path.dirname(os.path.abspath(__file__)),
    base_class_name='GenericBenchmarkBuilder',
    loader=benchmark_builder_loader)
