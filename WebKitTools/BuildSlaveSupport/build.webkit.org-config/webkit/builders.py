from webkit.factories import *

allWinBuilders = ['apple-slave-7', 'apple-slave-2'] # pewtermoose-slave-1
allMacPPCBuilders = ['apple-slave-1', 'apple-slave-3', 'apple-slave-6']
allMacIntelBuilders = ['bdash-slave-1', 'bdash-slave-2']
macIntelPixelBuilders = ['apple-slave-8']
allQtLinuxBuilders = ['webtroll-slave-1']
allQtWinBuilders = ['qt-slave-2']
allGtkLinuxBuilders = ['zecke-slave-1']
allWxMacBuilders = ['kollivier-slave-1']

# apple-slave-4 is currently giving incomprehensible ICEs when compiling:
# WebKit/History/WebBackForwardList.mm: In function 'WebHistoryItem* -[WebBackForwardList currentItem](WebBackForwardList*, objc_selector*)':
# WebKit/History/WebBackForwardList.mm:178: internal compiler error: Bus error

# apple-slave-5 is currently giving incomprehensible link errors:
# WebKitBuild/JavaScriptCore.build/Release/JavaScriptCore.build/Objects-normal/ppc/pcre_tables.o r_address (0x34a10d) field of relocation entry 12 in section (__DWARFA,__debug_info) out of range
# /usr/bin/libtool: internal link edit command failed


_builders = [('trunk-mac-ppc-release', StandardBuildFactory, allMacPPCBuilders, False),
             ('trunk-mac-intel-release', StandardBuildFactory, allMacIntelBuilders, False),
#             ('trunk-mac-ppc-debug', LeakBuildFactory, allMacPPCBuilders, False),
             ('trunk-mac-intel-debug', LeakBuildFactory, allMacIntelBuilders, False),
             ('trunk-mac-intel-pixel', PixelTestBuildFactory, macIntelPixelBuilders, False),
#             ('trunk-win-release', Win32BuildFactory, allWinBuilders, False),
             ('trunk-win-debug', Win32BuildFactory, allWinBuilders, False),
             ('trunk-qt-linux-release', QtBuildFactory, allQtLinuxBuilders, False),
             ('trunk-qt-win-release', QtBuildFactory, allQtWinBuilders, False),
             ('trunk-gtk-linux-release', GtkBuildFactory, allGtkLinuxBuilders, False),
             ('trunk-wx-mac-debug', WxBuildFactory, allWxMacBuilders, False),

#             ('trunk-mac-intel-coverage', CoverageDataBuildFactory, ['bdash-slave-1'], True),
#             ('trunk-mac-intel-nosvg', NoSVGBuildFactory, ['bdash-slave-2'], True),

#             ('stable-mac-ppc-release', StandardBuildFactory, allMacPPCBuilders, False),
#             ('stable-mac-intel-release', StandardBuildFactory, allMacIntelBuilders, False),
#             ('stable-mac-ppc-debug', LeakBuildFactory, allMacPPCBuilders, False),
#             ('stable-mac-intel-debug', LeakBuildFactory, allMacIntelBuilders, False),
#             ('stable-win-release', Win32BuildFactory, allWinBuilders, False),
             ]

def getBuilders():
    result = []
    for name, factory, slaves, periodic in _builders:
        result.append({'name': name,
                       'slavenames': slaves,
                       'builddir': name,
                       'factory': factory(),
                       'periodic': periodic})
    return result
