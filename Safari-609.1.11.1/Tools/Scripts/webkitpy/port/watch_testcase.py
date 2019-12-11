from webkitpy.common.version import Version
from webkitpy.port import darwin_testcase
from webkitpy.tool.mocktool import MockOptions


class WatchTest(darwin_testcase.DarwinTest):
    disable_setup = True

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=Version(5), **kwargs):
        if options:
            options.architecture = 'x86'
            options.webkit_test_runner = True
        else:
            options = MockOptions(architecture='x86', webkit_test_runner=True, configuration='Release')
        port = super(WatchTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, os_version=None, kwargs=kwargs)
        port.set_option('version', str(os_version))
        return port

    def test_driver_name(self):
        self.assertEqual(self.make_port().driver_name(), 'WebKitTestRunnerApp.app')

    def test_baseline_searchpath(self):
        search_path = self.make_port().default_baseline_search_path()
        self.assertEqual(search_path[-1], '/mock-checkout/LayoutTests/platform/wk2')
        self.assertEqual(search_path[-2], '/mock-checkout/LayoutTests/platform/watchos')
