from webkit.factories import *
from buildbot import locks

# There are four build slaves that take care of the majority of builds, with two other specialist slaves at Apple
# Slave 1 isn older G4 PowerMac dedicated to the PLT builds, as it needs extra configuration
# Slave 2 is a Windows PC dedicated to the Windows builds
# Slaves 3 and 4 are older G4 PowerMacs with relatively low amounts of RAM which leads to insanely slow leaks tests
# Slaves 4 and 5 are newer G5 PowerMacs with ATI graphics cards that lead to kernel panics during pixel tests

nonATIPowerPCBuilders = ['apple-slave-%d' % i for i in (3, 4)]
ATIPowerPCBuilders = ['apple-slave-%d' % i for i in (5, 6)]
allPowerPCBuilders = nonATIPowerPCBuilders + ATIPowerPCBuilders

_builders = [('post-commit-powerpc-mac-os-x', StandardBuildFactory, allPowerPCBuilders),
             ('post-commit-leaks-powerpc-mac-os-x', LeakBuildFactory, ATIPowerPCBuilders),
             ('page-layout-test-mac-os-x', PageLoadTestBuildFactory, ['apple-slave-1']),
             ('post-commit-pixel-powerpc-mac-os-x', PixelTestBuildFactory, nonATIPowerPCBuilders),
             ('post-commit-win32', Win32BuildFactory, ['apple-slave-2']),
             ('periodic-powerpc-mac-os-x-no-svg', NoSVGBuildFactory, allPowerPCBuilders),
             ('post-commit-linux-qt', StandardBuildFactory, ['wildfox-slave-1']),
             ]

def getBuilders():
    result = []
    oneBuildPerSlave = locks.SlaveLock('one-build-per-slave')
    for name, factory, slaves in _builders:
        result.append({'name': name,
                       'slavenames': slaves,
                       'builddir': name,
                       'locks': [oneBuildPerSlave],
                       'factory': factory()})
    return result
