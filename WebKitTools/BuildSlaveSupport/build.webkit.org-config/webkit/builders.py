from webkit.factories import *

_builders = [('post-commit-powerpc-mac-os-x', StandardBuildFactory, ['apple-slave-6', 'apple-slave-5']),
             ('post-commit-leaks-powerpc-mac-os-x', LeakBuildFactory, ['apple-slave-5', 'apple-slave-6']),
             ('page-layout-test-mac-os-x', PageLoadTestBuildFactory, ['apple-slave-1']),
             ('post-commit-pixel-powerpc-mac-os-x', PixelTestBuildFactory, ['apple-slave-3', 'apple-slave-4']),
             ('post-commit-win32', Win32BuildFactory, ['apple-slave-2']),
             ('periodic-powerpc-mac-os-x-no-svg', NoSVGBuildFactory, ['apple-slave-4', 'apple-slave-3']),
             ('post-commit-linux-qt', QTBuildFactory, ['wildfox-slave-1']),
             ]

def getBuilders():
    result = []
    for name, factory, slaves in _builders:
        result.append({'name': name,
                       'slavenames': slaves,
                       'builddir': name,
                       'factory': factory()})
    return result
