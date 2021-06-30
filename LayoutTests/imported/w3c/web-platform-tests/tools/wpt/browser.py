import os
import platform
import re
import shutil
import stat
import subprocess
import tempfile
from abc import ABCMeta, abstractmethod
from datetime import datetime, timedelta
from distutils.spawn import find_executable

from urllib.parse import urlsplit
import requests

from .utils import call, get, rmtree, untar, unzip, get_download_to_descriptor, sha256sum

uname = platform.uname()

# the rootUrl for the firefox-ci deployment of Taskcluster
FIREFOX_CI_ROOT_URL = 'https://firefox-ci-tc.services.mozilla.com'


def _get_fileversion(binary, logger=None):
    command = "(Get-Item '%s').VersionInfo.FileVersion" % binary.replace("'", "''")
    try:
        return call("powershell.exe", command).strip()
    except (subprocess.CalledProcessError, OSError):
        if logger is not None:
            logger.warning("Failed to call %s in PowerShell" % command)
        return None


def get_ext(filename):
    """Get the extension from a filename with special handling for .tar.foo"""
    name, ext = os.path.splitext(filename)
    if name.endswith(".tar"):
        ext = ".tar%s" % ext
    return ext


def get_taskcluster_artifact(index, path):
    TC_INDEX_BASE = FIREFOX_CI_ROOT_URL + "/api/index/v1/"

    resp = get(TC_INDEX_BASE + "task/%s/artifacts/%s" % (index, path))
    resp.raise_for_status()

    return resp


class Browser(object):
    __metaclass__ = ABCMeta

    def __init__(self, logger):
        self.logger = logger

    def _get_dest(self, dest, channel):
        if dest is None:
            # os.getcwd() doesn't include the venv path
            dest = os.path.join(os.getcwd(), "_venv")

        dest = os.path.join(dest, "browsers", channel)

        if not os.path.exists(dest):
            os.makedirs(dest)

        return dest

    @abstractmethod
    def download(self, dest=None, channel=None, rename=None):
        """Download a package or installer for the browser
        :param dest: Directory in which to put the dowloaded package
        :param channel: Browser channel to download
        :param rename: Optional name for the downloaded package; the original
                       extension is preserved.
        :return: The path to the downloaded package/installer
        """
        return NotImplemented

    @abstractmethod
    def install(self, dest=None, channel=None):
        """Download and install the browser.

        This method usually calls download().

        :param dest: Directory in which to install the browser
        :param channel: Browser channel to install
        :return: The path to the installed browser
        """
        return NotImplemented

    @abstractmethod
    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        """Download and install the WebDriver implementation for this browser.

        :param dest: Directory in which to install the WebDriver
        :param channel: Browser channel to install
        :param browser_binary: The path to the browser binary
        :return: The path to the installed WebDriver
        """
        return NotImplemented

    @abstractmethod
    def find_binary(self, venv_path=None, channel=None):
        """Find the binary of the browser.

        If the WebDriver for the browser is able to find the binary itself, this
        method doesn't need to be implemented, in which case NotImplementedError
        is suggested to be raised to prevent accidental use.
        """
        return NotImplemented

    @abstractmethod
    def find_webdriver(self, venv_path=None, channel=None):
        """Find the binary of the WebDriver."""
        return NotImplemented

    @abstractmethod
    def version(self, binary=None, webdriver_binary=None):
        """Retrieve the release version of the installed browser."""
        return NotImplemented

    @abstractmethod
    def requirements(self):
        """Name of the browser-specific wptrunner requirements file"""
        return NotImplemented


class Firefox(Browser):
    """Firefox-specific interface.

    Includes installation, webdriver installation, and wptrunner setup methods.
    """

    product = "firefox"
    binary = "browsers/firefox/firefox"
    requirements = "requirements_firefox.txt"

    platform = {
        "Linux": "linux",
        "Windows": "win",
        "Darwin": "macos"
    }.get(uname[0])

    application_name = {
        "stable": "Firefox.app",
        "beta": "Firefox.app",
        "nightly": "Firefox Nightly.app"
    }

    def platform_string_geckodriver(self):
        if self.platform is None:
            raise ValueError("Unable to construct a valid Geckodriver package name for current platform")

        if self.platform in ("linux", "win"):
            bits = "64" if uname[4] == "x86_64" else "32"
        else:
            bits = ""

        return "%s%s" % (self.platform, bits)

    def download(self, dest=None, channel="nightly", rename=None):
        product = {
            "nightly": "firefox-nightly-latest-ssl",
            "beta": "firefox-beta-latest-ssl",
            "stable": "firefox-latest-ssl"
        }

        os_builds = {
            ("linux", "x86"): "linux",
            ("linux", "x86_64"): "linux64",
            ("win", "x86"): "win",
            ("win", "AMD64"): "win64",
            ("macos", "x86_64"): "osx",
        }
        os_key = (self.platform, uname[4])

        if dest is None:
            dest = self._get_dest(None, channel)

        if channel not in product:
            raise ValueError("Unrecognised release channel: %s" % channel)

        if os_key not in os_builds:
            raise ValueError("Unsupported platform: %s %s" % os_key)

        url = "https://download.mozilla.org/?product=%s&os=%s&lang=en-US" % (product[channel],
                                                                             os_builds[os_key])
        self.logger.info("Downloading Firefox from %s" % url)
        resp = get(url)

        filename = None

        content_disposition = resp.headers.get('content-disposition')
        if content_disposition:
            filenames = re.findall("filename=(.+)", content_disposition)
            if filenames:
                filename = filenames[0]

        if not filename:
            filename = urlsplit(resp.url).path.rsplit("/", 1)[1]

        if not filename:
            filename = "firefox.tar.bz2"

        if rename:
            filename = "%s%s" % (rename, get_ext(filename))

        installer_path = os.path.join(dest, filename)

        with open(installer_path, "wb") as f:
            f.write(resp.content)

        return installer_path

    def install(self, dest=None, channel="nightly"):
        """Install Firefox."""
        import mozinstall

        dest = self._get_dest(dest, channel)

        filename = os.path.basename(dest)

        installer_path = self.download(dest, channel)

        try:
            mozinstall.install(installer_path, dest)
        except mozinstall.mozinstall.InstallError:
            if self.platform == "macos" and os.path.exists(os.path.join(dest, self.application_name.get(channel, "Firefox Nightly.app"))):
                # mozinstall will fail if nightly is already installed in the venv because
                # mac installation uses shutil.copy_tree
                mozinstall.uninstall(os.path.join(dest, self.application_name.get(channel, "Firefox Nightly.app")))
                mozinstall.install(filename, dest)
            else:
                raise

        os.remove(installer_path)
        return self.find_binary_path(dest)

    def find_binary_path(self, path=None, channel="nightly"):
        """Looks for the firefox binary in the virtual environment"""

        if path is None:
            # os.getcwd() doesn't include the venv path
            path = os.path.join(os.getcwd(), "_venv", "browsers", channel)

        binary = None

        if self.platform == "linux":
            binary = find_executable("firefox", os.path.join(path, "firefox"))
        elif self.platform == "win":
            import mozinstall
            try:
                binary = mozinstall.get_binary(path, "firefox")
            except mozinstall.InvalidBinary:
                # ignore the case where we fail to get a binary
                pass
        elif self.platform == "macos":
            binary = find_executable("firefox", os.path.join(path, self.application_name.get(channel, "Firefox Nightly.app"),
                                                             "Contents", "MacOS"))

        return binary

    def find_binary(self, venv_path=None, channel="nightly"):
        if venv_path is None:
            venv_path = os.path.join(os.getcwd(), "_venv")

        path = os.path.join(venv_path, "browsers", channel)
        binary = self.find_binary_path(path, channel)

        if not binary and self.platform == "win":
            winpaths = [os.path.expandvars("$SYSTEMDRIVE\\Program Files\\Mozilla Firefox"),
                        os.path.expandvars("$SYSTEMDRIVE\\Program Files (x86)\\Mozilla Firefox")]
            for winpath in winpaths:
                binary = self.find_binary_path(winpath, channel)
                if binary is not None:
                    break

        if not binary and self.platform == "macos":
            macpaths = ["/Applications/Firefox Nightly.app/Contents/MacOS",
                        os.path.expanduser("~/Applications/Firefox Nightly.app/Contents/MacOS"),
                        "/Applications/Firefox Developer Edition.app/Contents/MacOS",
                        os.path.expanduser("~/Applications/Firefox Developer Edition.app/Contents/MacOS"),
                        "/Applications/Firefox.app/Contents/MacOS",
                        os.path.expanduser("~/Applications/Firefox.app/Contents/MacOS")]
            return find_executable("firefox", os.pathsep.join(macpaths))

        if binary is None:
            return find_executable("firefox")

        return binary

    def find_certutil(self):
        path = find_executable("certutil")
        if path is None:
            return None
        if os.path.splitdrive(os.path.normcase(path))[1].split(os.path.sep) == ["", "windows", "system32", "certutil.exe"]:
            return None
        return path

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("geckodriver")

    def get_version_and_channel(self, binary):
        version_string = call(binary, "--version").strip()
        m = re.match(r"Mozilla Firefox (\d+\.\d+(?:\.\d+)?)(a|b)?", version_string)
        if not m:
            return None, "nightly"
        version, status = m.groups()
        channel = {"a": "nightly", "b": "beta"}
        return version, channel.get(status, "stable")

    def get_profile_bundle_url(self, version, channel):
        if channel == "stable":
            repo = "https://hg.mozilla.org/releases/mozilla-release"
            tag = "FIREFOX_%s_RELEASE" % version.replace(".", "_")
        elif channel == "beta":
            repo = "https://hg.mozilla.org/releases/mozilla-beta"
            major_version = version.split(".", 1)[0]
            # For beta we have a different format for betas that are now in stable releases
            # vs those that are not
            tags = get("https://hg.mozilla.org/releases/mozilla-beta/json-tags").json()["tags"]
            tags = {item["tag"] for item in tags}
            end_tag = "FIREFOX_BETA_%s_END" % major_version
            if end_tag in tags:
                tag = end_tag
            else:
                tag = "tip"
        else:
            repo = "https://hg.mozilla.org/mozilla-central"
            # Always use tip as the tag for nightly; this isn't quite right
            # but to do better we need the actual build revision, which we
            # can get if we have an application.ini file
            tag = "tip"

        return "%s/archive/%s.zip/testing/profiles/" % (repo, tag)

    def install_prefs(self, binary, dest=None, channel=None):
        if binary:
            version, channel_ = self.get_version_and_channel(binary)
            if channel is not None and channel != channel_:
                # Beta doesn't always seem to have the b in the version string, so allow the
                # manually supplied value to override the one from the binary
                self.logger.warning("Supplied channel doesn't match binary, using supplied channel")
            elif channel is None:
                channel = channel_
        else:
            version = None

        if dest is None:
            dest = os.curdir

        dest = os.path.join(dest, "profiles", channel)
        if version:
            dest = os.path.join(dest, version)
        have_cache = False
        if os.path.exists(dest) and len(os.listdir(dest)) > 0:
            if channel != "nightly":
                have_cache = True
            else:
                now = datetime.now()
                have_cache = (datetime.fromtimestamp(os.stat(dest).st_mtime) >
                              now - timedelta(days=1))

        # If we don't have a recent download, grab and extract the latest one
        if not have_cache:
            if os.path.exists(dest):
                rmtree(dest)
            os.makedirs(dest)

            url = self.get_profile_bundle_url(version, channel)

            self.logger.info("Installing test prefs from %s" % url)
            try:
                extract_dir = tempfile.mkdtemp()
                unzip(get(url).raw, dest=extract_dir)

                profiles = os.path.join(extract_dir, os.listdir(extract_dir)[0], 'testing', 'profiles')
                for name in os.listdir(profiles):
                    path = os.path.join(profiles, name)
                    shutil.move(path, dest)
            finally:
                rmtree(extract_dir)
        else:
            self.logger.info("Using cached test prefs from %s" % dest)

        return dest

    def _latest_geckodriver_version(self):
        """Get and return latest version number for geckodriver."""
        # This is used rather than an API call to avoid rate limits
        tags = call("git", "ls-remote", "--tags", "--refs",
                    "https://github.com/mozilla/geckodriver.git")
        release_re = re.compile(r".*refs/tags/v(\d+)\.(\d+)\.(\d+)")
        latest_release = (0, 0, 0)
        for item in tags.split("\n"):
            m = release_re.match(item)
            if m:
                version = tuple(int(item) for item in m.groups())
                if version > latest_release:
                    latest_release = version
        assert latest_release != (0, 0, 0)
        return "v%s.%s.%s" % tuple(str(item) for item in latest_release)

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        """Install latest Geckodriver."""
        if dest is None:
            dest = os.getcwd()

        path = None
        if channel == "nightly":
            path = self.install_geckodriver_nightly(dest)
            if path is None:
                self.logger.warning("Nightly webdriver not found; falling back to release")

        if path is None:
            version = self._latest_geckodriver_version()
            format = "zip" if uname[0] == "Windows" else "tar.gz"
            self.logger.debug("Latest geckodriver release %s" % version)
            url = ("https://github.com/mozilla/geckodriver/releases/download/%s/geckodriver-%s-%s.%s" %
                   (version, version, self.platform_string_geckodriver(), format))
            if format == "zip":
                unzip(get(url).raw, dest=dest)
            else:
                untar(get(url).raw, dest=dest)
            path = find_executable(os.path.join(dest, "geckodriver"))

        assert path is not None
        self.logger.info("Installed %s" %
                         subprocess.check_output([path, "--version"]).splitlines()[0])
        return path

    def install_geckodriver_nightly(self, dest):
        self.logger.info("Attempting to install webdriver from nightly")

        platform_bits = ("64" if uname[4] == "x86_64" else
                         ("32" if self.platform == "win" else ""))
        tc_platform = "%s%s" % (self.platform, platform_bits)

        archive_ext = ".zip" if uname[0] == "Windows" else ".tar.gz"
        archive_name = "public/build/geckodriver%s" % archive_ext

        try:
            resp = get_taskcluster_artifact(
                "gecko.v2.mozilla-central.latest.geckodriver.%s" % tc_platform,
                archive_name)
        except Exception:
            self.logger.info("Geckodriver download failed")
            return

        if archive_ext == ".zip":
            unzip(resp.raw, dest)
        else:
            untar(resp.raw, dest)

        exe_ext = ".exe" if uname[0] == "Windows" else ""
        path = os.path.join(dest, "geckodriver%s" % exe_ext)

        self.logger.info("Extracted geckodriver to %s" % path)

        return path

    def version(self, binary=None, webdriver_binary=None):
        """Retrieve the release version of the installed browser."""
        version_string = call(binary, "--version").strip()
        m = re.match(r"Mozilla Firefox (.*)", version_string)
        if not m:
            return None
        return m.group(1)


class FirefoxAndroid(Browser):
    """Android-specific Firefox interface."""

    product = "firefox_android"
    requirements = "requirements_firefox.txt"

    def __init__(self, logger):
        super(FirefoxAndroid, self).__init__(logger)
        self.apk_path = None

    def download(self, dest=None, channel=None, rename=None):
        if dest is None:
            dest = os.pwd

        resp = get_taskcluster_artifact(
            "gecko.v2.mozilla-central.latest.mobile.android-x86_64-opt",
            "public/build/geckoview-androidTest.apk")

        filename = "geckoview-androidTest.apk"
        if rename:
            filename = "%s%s" % (rename, get_ext(filename)[1])
        self.apk_path = os.path.join(dest, filename)

        with open(self.apk_path, "wb") as f:
            f.write(resp.content)

        return self.apk_path

    def install(self, dest=None, channel=None):
        return self.download(dest, channel)

    def install_prefs(self, binary, dest=None, channel=None):
        fx_browser = Firefox(self.logger)
        return fx_browser.install_prefs(binary, dest, channel)

    def find_binary(self, venv_path=None, channel=None):
        return self.apk_path

    def find_webdriver(self, venv_path=None, channel=None):
        raise NotImplementedError

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        return None


class Chrome(Browser):
    """Chrome-specific interface.

    Includes webdriver installation, and wptrunner setup methods.
    """

    product = "chrome"
    requirements = "requirements_chrome.txt"
    platforms = {
        "Linux": "Linux",
        "Windows": "Win",
        "Darwin": "Mac",
    }

    def __init__(self, logger):
        super(Chrome, self).__init__(logger)
        self._last_change = None

    def download(self, dest=None, channel=None, rename=None):
        if channel != "nightly":
            raise NotImplementedError("We can only download Chrome Nightly (Chromium ToT) for you.")
        if dest is None:
            dest = self._get_dest(None, channel)

        filename = self._chromium_package_name() + ".zip"
        url = self._latest_chromium_snapshot_url() + filename
        self.logger.info("Downloading Chrome from %s" % url)
        resp = get(url)
        installer_path = os.path.join(dest, filename)
        with open(installer_path, "wb") as f:
            f.write(resp.content)
        return installer_path

    def install(self, dest=None, channel=None):
        if channel != "nightly":
            raise NotImplementedError("We can only install Chrome Nightly (Chromium ToT) for you.")
        dest = self._get_dest(dest, channel)

        installer_path = self.download(dest, channel)
        with open(installer_path, "rb") as f:
            unzip(f, dest)
        os.remove(installer_path)
        return self.find_nightly_binary(dest)

    def install_mojojs(self, dest, channel, browser_binary):
        if channel == "nightly" or channel == "canary":
            url = self._latest_chromium_snapshot_url() + "mojojs.zip"
        else:
            chrome_version = self.version(binary=browser_binary)
            assert chrome_version, "Cannot determine the version of Chrome"
            # Remove channel suffixes (e.g. " dev").
            chrome_version = chrome_version.split(' ')[0]
            url = "https://storage.googleapis.com/chrome-wpt-mojom/%s/linux64/mojojs.zip" % chrome_version

        extracted = os.path.join(dest, "mojojs", "gen")
        last_url_file = os.path.join(extracted, "DOWNLOADED_FROM")
        if os.path.exists(last_url_file):
            with open(last_url_file, "rt") as f:
                last_url = f.read().strip()
            if last_url == url:
                self.logger.info("Mojo bindings already up to date")
                return extracted
            rmtree(extracted)

        self.logger.info("Downloading Mojo bindings from %s" % url)
        unzip(get(url).raw, dest)
        with open(last_url_file, "wt") as f:
            f.write(url)
        return extracted

    def _chromedriver_platform_string(self):
        platform = self.platforms.get(uname[0])

        if platform is None:
            raise ValueError("Unable to construct a valid Chrome package name for current platform")
        platform = platform.lower()

        if platform == "linux":
            bits = "64" if uname[4] == "x86_64" else "32"
        elif platform == "mac":
            bits = "64"
        elif platform == "win":
            bits = "32"

        return "%s%s" % (platform, bits)

    def _chromium_platform_string(self):
        platform = self.platforms.get(uname[0])

        if platform is None:
            raise ValueError("Unable to construct a valid Chromium package name for current platform")

        if (platform == "Linux" or platform == "Win") and uname[4] == "x86_64":
            platform += "_x64"

        return platform

    def _chromium_package_name(self):
        return "chrome-%s" % self.platforms.get(uname[0]).lower()

    def _latest_chromium_snapshot_url(self):
        # Make sure we use the same revision in an invocation.
        architecture = self._chromium_platform_string()
        if self._last_change is None:
            revision_url = "https://storage.googleapis.com/chromium-browser-snapshots/%s/LAST_CHANGE" % architecture
            self._last_change = get(revision_url).text.strip()
        return "https://storage.googleapis.com/chromium-browser-snapshots/%s/%s/" % (architecture, self._last_change)

    def find_nightly_binary(self, dest):
        if uname[0] == "Darwin":
            return find_executable("Chromium",
                                   os.path.join(dest, self._chromium_package_name(), "Chromium.app", "Contents", "MacOS"))
        # find_executable will add .exe on Windows automatically.
        return find_executable("chrome", os.path.join(dest, self._chromium_package_name()))

    def find_binary(self, venv_path=None, channel=None):
        if channel == "nightly":
            return self.find_nightly_binary(self._get_dest(venv_path, channel))

        if uname[0] == "Linux":
            name = "google-chrome"
            if channel == "stable":
                name += "-stable"
            elif channel == "beta":
                name += "-beta"
            elif channel == "dev":
                name += "-unstable"
            # No Canary on Linux.
            return find_executable(name)
        if uname[0] == "Darwin":
            suffix = ""
            if channel in ("beta", "dev", "canary"):
                suffix = " " + channel.capitalize()
            return "/Applications/Google Chrome%s.app/Contents/MacOS/Google Chrome%s" % (suffix, suffix)
        if uname[0] == "Windows":
            path = os.path.expandvars(r"$SYSTEMDRIVE\Program Files (x86)\Google\Chrome\Application\chrome.exe")
            if not os.path.exists(path):
                path = os.path.expandvars(r"$SYSTEMDRIVE\Program Files\Google\Chrome\Application\chrome.exe")
            return path
        self.logger.warning("Unable to find the browser binary.")
        return None

    def find_webdriver(self, venv_path=None, channel=None, browser_binary=None):
        return find_executable("chromedriver")

    def webdriver_supports_browser(self, webdriver_binary, browser_binary, browser_channel):
        chromedriver_version = self.webdriver_version(webdriver_binary)
        if not chromedriver_version:
            self.logger.warning(
                "Unable to get version for ChromeDriver %s, rejecting it" %
                webdriver_binary)
            return False

        browser_version = self.version(browser_binary)
        if not browser_version:
            # If we can't get the browser version, we just have to assume the
            # ChromeDriver is good.
            return True

        # Check that the ChromeDriver version matches the Chrome version.
        chromedriver_major = int(chromedriver_version.split('.')[0])
        browser_major = int(browser_version.split('.')[0])
        if chromedriver_major != browser_major:
            # There is no official ChromeDriver release for the dev channel -
            # it switches between beta and tip-of-tree, so we accept version+1
            # too for dev.
            if browser_channel == "dev" and chromedriver_major == (browser_major + 1):
                self.logger.debug(
                    "Accepting ChromeDriver %s for Chrome/Chromium Dev %s" %
                    (chromedriver_version, browser_version))
                return True
            self.logger.warning(
                "ChromeDriver %s does not match Chrome/Chromium %s" %
                (chromedriver_version, browser_version))
            return False
        return True

    def _official_chromedriver_url(self, chrome_version):
        # http://chromedriver.chromium.org/downloads/version-selection
        parts = chrome_version.split(".")
        assert len(parts) == 4
        latest_url = "https://chromedriver.storage.googleapis.com/LATEST_RELEASE_%s.%s.%s" % tuple(parts[:-1])
        try:
            latest = get(latest_url).text.strip()
        except requests.RequestException:
            latest_url = "https://chromedriver.storage.googleapis.com/LATEST_RELEASE_%s" % parts[0]
            try:
                latest = get(latest_url).text.strip()
            except requests.RequestException:
                return None
        return "https://chromedriver.storage.googleapis.com/%s/chromedriver_%s.zip" % (
            latest, self._chromedriver_platform_string())

    def _chromium_chromedriver_url(self, chrome_version):
        if chrome_version:
            try:
                # Try to find the Chromium build with the same revision.
                omaha = get("https://omahaproxy.appspot.com/deps.json?version=" + chrome_version).json()
                revision = omaha['chromium_base_position']
                url = "https://storage.googleapis.com/chromium-browser-snapshots/%s/%s/chromedriver_%s.zip" % (
                    self._chromium_platform_string(), revision, self._chromedriver_platform_string())
                # Check the status without downloading the content (this is a streaming request).
                get(url)
                return url
            except requests.RequestException:
                pass
        # Fall back to the tip-of-tree Chromium build.
        return "%schromedriver_%s.zip" % (self._latest_chromium_snapshot_url(), self._chromedriver_platform_string())

    def _latest_chromedriver_url(self, chrome_version):
        # Remove channel suffixes (e.g. " dev").
        chrome_version = chrome_version.split(' ')[0]
        return (self._official_chromedriver_url(chrome_version) or
                self._chromium_chromedriver_url(chrome_version))

    def install_webdriver_by_version(self, version, dest=None):
        if dest is None:
            dest = os.pwd

        # There may be an existing chromedriver binary from a previous install.
        # To provide a clean install experience, remove the old binary - this
        # avoids tricky issues like unzipping over a read-only file.
        existing_binary_path = find_executable("chromedriver", dest)
        if existing_binary_path:
            self.logger.info("Removing existing ChromeDriver binary: %s" %
                existing_binary_path)
            os.chmod(existing_binary_path, stat.S_IWUSR)
            os.remove(existing_binary_path)

        url = self._latest_chromedriver_url(version) if version \
            else self._chromium_chromedriver_url(None)
        self.logger.info("Downloading ChromeDriver from %s" % url)
        unzip(get(url).raw, dest)

        # The two sources of ChromeDriver have different zip structures:
        # * Chromium archives the binary inside a chromedriver_* directory;
        # * Chrome archives the binary directly.
        # We want to make sure the binary always ends up directly in bin/.
        chromedriver_dir = os.path.join(
            dest, 'chromedriver_%s' % self._chromedriver_platform_string())
        binary_path = find_executable("chromedriver", chromedriver_dir)
        if binary_path is not None:
            shutil.move(binary_path, dest)
            rmtree(chromedriver_dir)

        binary_path = find_executable("chromedriver", dest)
        assert binary_path is not None
        return binary_path

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        if channel == "nightly":
            # The "nightly" channel is not an official channel, so we simply download ToT.
            return self.install_webdriver_by_version(None, dest)

        if browser_binary is None:
            browser_binary = self.find_binary(channel)
        return self.install_webdriver_by_version(
            self.version(browser_binary), dest)

    def version(self, binary=None, webdriver_binary=None):
        if not binary:
            self.logger.warning("No browser binary provided.")
            return None

        if uname[0] == "Windows":
            return _get_fileversion(binary, self.logger)

        try:
            version_string = call(binary, "--version").strip()
        except (subprocess.CalledProcessError, OSError) as e:
            self.logger.warning("Failed to call %s: %s" % (binary, e))
            return None
        m = re.match(r"(?:Google Chrome|Chromium) (.*)", version_string)
        if not m:
            self.logger.warning("Failed to extract version from: %s" % version_string)
            return None
        return m.group(1)

    def webdriver_version(self, webdriver_binary):
        if uname[0] == "Windows":
            return _get_fileversion(webdriver_binary, self.logger)

        try:
            version_string = call(webdriver_binary, "--version").strip()
        except (subprocess.CalledProcessError, OSError) as e:
            self.logger.warning("Failed to call %s: %s" % (webdriver_binary, e))
            return None
        m = re.match(r"ChromeDriver ([0-9][0-9.]*)", version_string)
        if not m:
            self.logger.warning("Failed to extract version from: %s" % version_string)
            return None
        return m.group(1)


class ChromeAndroidBase(Browser):
    """A base class for ChromeAndroid and AndroidWebView.

    On Android, WebView is based on Chromium open source project, and on some
    versions of Android we share the library with Chrome. Therefore, we have
    a very similar WPT runner implementation.
    Includes webdriver installation.
    """
    __metaclass__ = ABCMeta  # This is an abstract class.

    def __init__(self, logger):
        super(ChromeAndroidBase, self).__init__(logger)
        self.device_serial = None

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    @abstractmethod
    def find_binary(self, venv_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("chromedriver")

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        if browser_binary is None:
            browser_binary = self.find_binary(channel)
        chrome = Chrome(self.logger)
        return chrome.install_webdriver_by_version(
            self.version(browser_binary), dest)

    def version(self, binary=None, webdriver_binary=None):
        if not binary:
            self.logger.warning("No package name provided.")
            return None

        command = ['adb']
        if self.device_serial:
            command.extend(['-s', self.device_serial])
        command.extend(['shell', 'dumpsys', 'package', binary])
        try:
            output = call(*command)
        except (subprocess.CalledProcessError, OSError):
            self.logger.warning("Failed to call %s" % " ".join(command))
            return None
        match = re.search(r'versionName=(.*)', output)
        if not match:
            self.logger.warning("Failed to find versionName")
            return None
        return match.group(1)


class ChromeAndroid(ChromeAndroidBase):
    """Chrome-specific interface for Android.
    """

    product = "chrome_android"
    requirements = "requirements_chrome_android.txt"

    def find_binary(self, venv_path=None, channel=None):
        if channel in ("beta", "dev", "canary"):
            return "com.chrome." + channel
        return "com.android.chrome"


# TODO(aluo): This is largely copied from the AndroidWebView implementation.
# Tests are not running for weblayer yet (crbug/1019521), this initial
# implementation will help to reproduce and debug any issues.
class AndroidWeblayer(ChromeAndroidBase):
    """Weblayer-specific interface for Android."""

    product = "android_weblayer"
    # TODO(aluo): replace this with weblayer version after tests are working.
    requirements = "requirements_android_webview.txt"

    def find_binary(self, venv_path=None, channel=None):
        return "org.chromium.weblayer.shell"


class AndroidWebview(ChromeAndroidBase):
    """Webview-specific interface for Android.

    Design doc:
    https://docs.google.com/document/d/19cGz31lzCBdpbtSC92svXlhlhn68hrsVwSB7cfZt54o/view
    """

    product = "android_webview"
    requirements = "requirements_android_webview.txt"

    def find_binary(self, venv_path=None, channel=None):
        # Just get the current package name of the WebView provider.
        # For WebView, it is not trivial to change the WebView provider, so
        # we will just grab whatever is available.
        # https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/channels.md
        command = ['adb']
        if self.device_serial:
            command.extend(['-s', self.device_serial])
        command.extend(['shell', 'dumpsys', 'webviewupdate'])
        try:
            output = call(*command)
        except (subprocess.CalledProcessError, OSError):
            self.logger.warning("Failed to call %s" % " ".join(command))
            return None
        m = re.search(r'^\s*Current WebView package \(name, version\): \((.*), ([0-9.]*)\)$',
                      output, re.M)
        if m is None:
            self.logger.warning("Unable to find current WebView package in dumpsys output")
            return None
        self.logger.warning("Final package name: " + m.group(1))
        return m.group(1)


class ChromeiOS(Browser):
    """Chrome-specific interface for iOS.
    """

    product = "chrome_ios"
    requirements = "requirements_chrome_ios.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        raise NotImplementedError

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        return None


class Opera(Browser):
    """Opera-specific interface.

    Includes webdriver installation, and wptrunner setup methods.
    """

    product = "opera"
    requirements = "requirements_opera.txt"

    @property
    def binary(self):
        if uname[0] == "Linux":
            return "/usr/bin/opera"
        # TODO Windows, Mac?
        self.logger.warning("Unable to find the browser binary.")
        return None

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def platform_string(self):
        platform = {
            "Linux": "linux",
            "Windows": "win",
            "Darwin": "mac"
        }.get(uname[0])

        if platform is None:
            raise ValueError("Unable to construct a valid Opera package name for current platform")

        if platform == "linux":
            bits = "64" if uname[4] == "x86_64" else "32"
        elif platform == "mac":
            bits = "64"
        elif platform == "win":
            bits = "32"

        return "%s%s" % (platform, bits)

    def find_binary(self, venv_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("operadriver")

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        if dest is None:
            dest = os.pwd
        latest = get("https://api.github.com/repos/operasoftware/operachromiumdriver/releases/latest").json()["tag_name"]
        url = "https://github.com/operasoftware/operachromiumdriver/releases/download/%s/operadriver_%s.zip" % (latest,
                                                                                                                self.platform_string())
        unzip(get(url).raw, dest)

        operadriver_dir = os.path.join(dest, "operadriver_%s" % self.platform_string())
        shutil.move(os.path.join(operadriver_dir, "operadriver"), dest)
        rmtree(operadriver_dir)

        path = find_executable("operadriver")
        st = os.stat(path)
        os.chmod(path, st.st_mode | stat.S_IEXEC)
        return path

    def version(self, binary=None, webdriver_binary=None):
        """Retrieve the release version of the installed browser."""
        binary = binary or self.binary
        try:
            output = call(binary, "--version")
        except subprocess.CalledProcessError:
            self.logger.warning("Failed to call %s" % binary)
            return None
        m = re.search(r"[0-9\.]+( [a-z]+)?$", output.strip())
        if m:
            return m.group(0)


class EdgeChromium(Browser):
    """MicrosoftEdge-specific interface."""
    platform = {
        "Linux": "linux",
        "Windows": "win",
        "Darwin": "macos"
    }.get(uname[0])
    product = "edgechromium"
    edgedriver_name = "msedgedriver"
    requirements = "requirements_edge_chromium.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        binary = None
        if self.platform == "win":
            binaryname = "msedge"
            binary = find_executable(binaryname)
            if not binary:
                # Use paths from different Edge channels starting with Release\Beta\Dev\Canary
                winpaths = [os.path.expanduser("~\\AppData\\Local\\Microsoft\\Edge\\Application"),
                            os.path.expandvars("$SYSTEMDRIVE\\Program Files\\Microsoft\\Edge\\Application"),
                            os.path.expandvars("$SYSTEMDRIVE\\Program Files\\Microsoft\\Edge Beta\\Application"),
                            os.path.expandvars("$SYSTEMDRIVE\\Program Files\\Microsoft\\Edge Dev\\Application"),
                            os.path.expandvars("$SYSTEMDRIVE\\Program Files (x86)\\Microsoft\\Edge\\Application"),
                            os.path.expandvars("$SYSTEMDRIVE\\Program Files (x86)\\Microsoft\\Edge Beta\\Application"),
                            os.path.expandvars("$SYSTEMDRIVE\\Program Files (x86)\\Microsoft\\Edge Dev\\Application"),
                            os.path.expanduser("~\\AppData\\Local\\Microsoft\\Edge SxS\\Application")]
                return find_executable(binaryname, os.pathsep.join(winpaths))
        if self.platform == "macos":
            binaryname = "Microsoft Edge Canary"
            binary = find_executable(binaryname)
            if not binary:
                macpaths = ["/Applications/Microsoft Edge.app/Contents/MacOS",
                            os.path.expanduser("~/Applications/Microsoft Edge.app/Contents/MacOS"),
                            "/Applications/Microsoft Edge Canary.app/Contents/MacOS",
                            os.path.expanduser("~/Applications/Microsoft Edge Canary.app/Contents/MacOS")]
                return find_executable("Microsoft Edge Canary", os.pathsep.join(macpaths))
        return binary

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("msedgedriver")

    def webdriver_supports_browser(self, webdriver_binary, browser_binary):
        edgedriver_version = self.webdriver_version(webdriver_binary)
        if not edgedriver_version:
            self.logger.warning(
                "Unable to get version for EdgeDriver %s, rejecting it" %
                webdriver_binary)
            return False

        browser_version = self.version(browser_binary)
        if not browser_version:
            # If we can't get the browser version, we just have to assume the
            # EdgeDriver is good.
            return True

        # Check that the EdgeDriver version matches the Edge version.
        edgedriver_major = edgedriver_version.split('.')[0]
        browser_major = browser_version.split('.')[0]
        if edgedriver_major != browser_major:
            self.logger.warning("EdgeDriver %s does not match Edge %s" %
                                (edgedriver_version, browser_version))
            return False
        return True

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        if self.platform != "win" and self.platform != "macos":
            raise ValueError("Only Windows and Mac platforms are currently supported")

        if dest is None:
            dest = os.pwd

        if channel is None:
            version_url = "https://msedgedriver.azureedge.net/LATEST_DEV"
        else:
            version_url = "https://msedgedriver.azureedge.net/LATEST_%s" % channel.upper()
        version = get(version_url).text.strip()

        if self.platform == "macos":
            bits = "mac64"
            edgedriver_path = os.path.join(dest, self.edgedriver_name)
        else:
            bits = "win64" if uname[4] == "x86_64" else "win32"
            edgedriver_path = os.path.join(dest, "%s.exe" % self.edgedriver_name)
        url = "https://msedgedriver.azureedge.net/%s/edgedriver_%s.zip" % (version, bits)

        # cleanup existing Edge driver files to avoid access_denied errors when unzipping
        if os.path.isfile(edgedriver_path):
            # remove read-only attribute
            os.chmod(edgedriver_path, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)  # 0777
            print("Delete %s file" % edgedriver_path)
            os.remove(edgedriver_path)
        driver_notes_path = os.path.join(dest, "Driver_notes")
        if os.path.isdir(driver_notes_path):
            print("Delete %s folder" % driver_notes_path)
            rmtree(driver_notes_path)

        self.logger.info("Downloading MSEdgeDriver from %s" % url)
        unzip(get(url).raw, dest)
        if os.path.isfile(edgedriver_path):
            self.logger.info("Successfully downloaded MSEdgeDriver to %s" % edgedriver_path)
        return find_executable(self.edgedriver_name, dest)

    def version(self, binary=None, webdriver_binary=None):
        if binary is None:
            binary = self.find_binary()
        if self.platform != "win":
            try:
                version_string = call(binary, "--version").strip()
            except subprocess.CalledProcessError:
                self.logger.warning("Failed to call %s" % binary)
                return None
            m = re.match(r"Microsoft Edge (.*) ", version_string)
            if not m:
                self.logger.warning("Failed to extract version from: %s" % version_string)
                return None
            return m.group(1)
        else:
            if binary is not None:
                return _get_fileversion(binary, self.logger)
            self.logger.warning("Failed to find Edge binary.")
            return None

    def webdriver_version(self, webdriver_binary):
        if self.platform == "win":
            return _get_fileversion(webdriver_binary, self.logger)

        try:
            version_string = call(webdriver_binary, "--version").strip()
        except subprocess.CalledProcessError:
            self.logger.warning("Failed to call %s" % webdriver_binary)
            return None
        m = re.match(r"MSEdgeDriver ([0-9][0-9.]*)", version_string)
        if not m:
            self.logger.warning("Failed to extract version from: %s" % version_string)
            return None
        return m.group(1)


class Edge(Browser):
    """Edge-specific interface."""

    product = "edge"
    requirements = "requirements_edge.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("MicrosoftWebDriver")

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        command = "(Get-AppxPackage Microsoft.MicrosoftEdge).Version"
        try:
            return call("powershell.exe", command).strip()
        except (subprocess.CalledProcessError, OSError):
            self.logger.warning("Failed to call %s in PowerShell" % command)
            return None


class EdgeWebDriver(Edge):
    product = "edge_webdriver"


class InternetExplorer(Browser):
    """Internet Explorer-specific interface."""

    product = "ie"
    requirements = "requirements_ie.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("IEDriverServer.exe")

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        return None


class Safari(Browser):
    """Safari-specific interface.

    Includes installation, webdriver installation, and wptrunner setup methods.
    """

    product = "safari"
    requirements = "requirements_safari.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        path = None
        if channel == "preview":
            path = "/Applications/Safari Technology Preview.app/Contents/MacOS"
        return find_executable("safaridriver", path)

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        if webdriver_binary is None:
            self.logger.warning("Cannot find Safari version without safaridriver")
            return None
        # Use `safaridriver --version` to get the version. Example output:
        # "Included with Safari 12.1 (14607.1.11)"
        # "Included with Safari Technology Preview (Release 67, 13607.1.9.0.1)"
        # The `--version` flag was added in STP 67, so allow the call to fail.
        try:
            version_string = call(webdriver_binary, "--version").strip()
        except subprocess.CalledProcessError:
            self.logger.warning("Failed to call %s --version" % webdriver_binary)
            return None
        m = re.match(r"Included with Safari (.*)", version_string)
        if not m:
            self.logger.warning("Failed to extract version from: %s" % version_string)
            return None
        return m.group(1)


class Servo(Browser):
    """Servo-specific interface."""

    product = "servo"
    requirements = "requirements_servo.txt"

    def platform_components(self):
        platform = {
            "Linux": "linux",
            "Windows": "win",
            "Darwin": "mac"
        }.get(uname[0])

        if platform is None:
            raise ValueError("Unable to construct a valid Servo package for current platform")

        if platform == "linux":
            extension = ".tar.gz"
            decompress = untar
        elif platform == "win" or platform == "mac":
            raise ValueError("Unable to construct a valid Servo package for current platform")

        return (platform, extension, decompress)

    def _get(self, channel="nightly"):
        if channel != "nightly":
            raise ValueError("Only nightly versions of Servo are available")

        platform, extension, _ = self.platform_components()
        url = "https://download.servo.org/nightly/%s/servo-latest%s" % (platform, extension)
        return get(url)

    def download(self, dest=None, channel="nightly", rename=None):
        if dest is None:
            dest = os.pwd

        resp = self._get(dest, channel)
        _, extension, _ = self.platform_components()

        filename = rename if rename is not None else "servo-latest"
        with open(os.path.join(dest, "%s%s" % (filename, extension,)), "w") as f:
            f.write(resp.content)

    def install(self, dest=None, channel="nightly"):
        """Install latest Browser Engine."""
        if dest is None:
            dest = os.pwd

        _, _, decompress = self.platform_components()

        resp = self._get(channel)
        decompress(resp.raw, dest=dest)
        path = find_executable("servo", os.path.join(dest, "servo"))
        st = os.stat(path)
        os.chmod(path, st.st_mode | stat.S_IEXEC)
        return path

    def find_binary(self, venv_path=None, channel=None):
        path = find_executable("servo", os.path.join(venv_path, "servo"))
        if path is None:
            path = find_executable("servo")
        return path

    def find_webdriver(self, venv_path=None, channel=None):
        return None

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        """Retrieve the release version of the installed browser."""
        output = call(binary, "--version")
        m = re.search(r"Servo ([0-9\.]+-[a-f0-9]+)?(-dirty)?$", output.strip())
        if m:
            return m.group(0)


class ServoWebDriver(Servo):
    product = "servodriver"


class Sauce(Browser):
    """Sauce-specific interface."""

    product = "sauce"
    requirements = "requirements_sauce.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venev_path=None, channel=None):
        raise NotImplementedError

    def find_webdriver(self, venv_path=None, channel=None):
        raise NotImplementedError

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        return None


class WebKit(Browser):
    """WebKit-specific interface."""

    product = "webkit"
    requirements = "requirements_webkit.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        return None

    def find_webdriver(self, venv_path=None, channel=None):
        return None

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        return None


class WebKitGTKMiniBrowser(WebKit):


    def _get_osidversion(self):
        with open('/etc/os-release', 'r') as osrelease_handle:
            for line in osrelease_handle.readlines():
                if line.startswith('ID='):
                    os_id = line.split('=')[1].strip().strip('"')
                if line.startswith('VERSION_ID='):
                    version_id = line.split('=')[1].strip().strip('"')
        assert(os_id)
        assert(version_id)
        osidversion = os_id + '-' + version_id
        assert(' ' not in osidversion)
        assert(len(osidversion) > 3)
        return osidversion.capitalize()


    def download(self, dest=None, channel=None, rename=None):
        base_dowload_uri = "https://webkitgtk.org/built-products/"
        base_download_dir = base_dowload_uri + "x86_64/release/" + channel + "/" + self._get_osidversion() + "/MiniBrowser/"
        try:
            response = get(base_download_dir + "LAST-IS")
        except requests.exceptions.HTTPError as e:
            if e.response.status_code == 404:
                raise RuntimeError("Can't find a WebKitGTK MiniBrowser %s bundle for %s at %s"
                                   % (channel, self._get_osidversion(), base_dowload_uri))
            raise

        bundle_filename = response.text.strip()
        bundle_url = base_download_dir + bundle_filename

        if dest is None:
            dest = self._get_dest(None, channel)
        bundle_file_path = os.path.join(dest, bundle_filename)

        self.logger.info("Downloading WebKitGTK MiniBrowser bundle from %s" % bundle_url)
        with open(bundle_file_path, "w+b") as f:
            get_download_to_descriptor(f, bundle_url)

        bundle_filename_no_ext, _ = os.path.splitext(bundle_filename)
        bundle_hash_url = base_download_dir + bundle_filename_no_ext + ".sha256sum"
        bundle_expected_hash = get(bundle_hash_url).text.strip().split(" ")[0]
        bundle_computed_hash = sha256sum(bundle_file_path)

        if bundle_expected_hash != bundle_computed_hash:
            self.logger.error("Calculated SHA256 hash is %s but was expecting %s" % (bundle_computed_hash,bundle_expected_hash))
            raise RuntimeError("The WebKitGTK MiniBrowser bundle at %s has incorrect SHA256 hash." % bundle_file_path)
        return bundle_file_path

    def install(self, dest=None, channel=None, prompt=True):
        dest = self._get_dest(dest, channel)
        bundle_path = self.download(dest, channel)
        bundle_uncompress_directory = os.path.join(dest, "webkitgtk_minibrowser")

        # Clean it from previous runs
        if os.path.exists(bundle_uncompress_directory):
            rmtree(bundle_uncompress_directory)
        os.mkdir(bundle_uncompress_directory)

        with open(bundle_path, "rb") as f:
            unzip(f, bundle_uncompress_directory)

        install_dep_script = os.path.join(bundle_uncompress_directory, "install-dependencies.sh")
        if os.path.isfile(install_dep_script):
            self.logger.info("Executing install-dependencies.sh script from bundle.")
            install_dep_cmd = [install_dep_script]
            if not prompt:
                install_dep_cmd.append("--autoinstall")
            # use subprocess.check_call() directly to display unbuffered stdout/stderr in real-time.
            subprocess.check_call(install_dep_cmd)

        minibrowser_path = os.path.join(bundle_uncompress_directory, "MiniBrowser")
        if not os.path.isfile(minibrowser_path):
            raise RuntimeError("Can't find a MiniBrowser binary at %s" % minibrowser_path)

        os.remove(bundle_path)
        install_ok_file = os.path.join(bundle_uncompress_directory, ".installation-ok")
        open(install_ok_file, "w").close()  # touch
        self.logger.info("WebKitGTK MiniBrowser bundle for channel %s installed." % channel)
        return minibrowser_path

    def _find_executable_in_channel_bundle(self, binary, venv_path=None, channel=None):
        if venv_path:
            venv_base_path = self._get_dest(venv_path, channel)
            bundle_dir = os.path.join(venv_base_path, "webkitgtk_minibrowser")
            install_ok_file = os.path.join(bundle_dir, ".installation-ok")
            if os.path.isfile(install_ok_file):
                return find_executable(binary, bundle_dir)
        return None


    def find_binary(self, venv_path=None, channel=None):
        minibrowser_path = self._find_executable_in_channel_bundle("MiniBrowser", venv_path, channel)
        if minibrowser_path:
            return minibrowser_path

        libexecpaths = ["/usr/libexec/webkit2gtk-4.0"]  # Fedora path
        triplet = "x86_64-linux-gnu"
        # Try to use GCC to detect this machine triplet
        gcc = find_executable("gcc")
        if gcc:
            try:
                triplet = call(gcc, "-dumpmachine").strip()
            except subprocess.CalledProcessError:
                pass
        # Add Debian/Ubuntu path
        libexecpaths.append("/usr/lib/%s/webkit2gtk-4.0" % triplet)
        return find_executable("MiniBrowser", os.pathsep.join(libexecpaths))

    def find_webdriver(self, venv_path=None, channel=None):
        webdriver_path = self._find_executable_in_channel_bundle("WebKitWebDriver", venv_path, channel)
        if not webdriver_path:
            webdriver_path = find_executable("WebKitWebDriver")
        return webdriver_path

    def version(self, binary=None, webdriver_binary=None):
        if binary is None:
            return None
        try:  # WebKitGTK MiniBrowser before 2.26.0 doesn't support --version
            output = call(binary, "--version").strip()
        except subprocess.CalledProcessError:
            return None
        # Example output: "WebKitGTK 2.26.1"
        if output:
            m = re.match(r"WebKitGTK (.+)", output)
            if not m:
                self.logger.warning("Failed to extract version from: %s" % output)
                return None
            return m.group(1)
        return None


class Epiphany(Browser):
    """Epiphany-specific interface."""

    product = "epiphany"
    requirements = "requirements_epiphany.txt"

    def download(self, dest=None, channel=None, rename=None):
        raise NotImplementedError

    def install(self, dest=None, channel=None):
        raise NotImplementedError

    def find_binary(self, venv_path=None, channel=None):
        return find_executable("epiphany")

    def find_webdriver(self, venv_path=None, channel=None):
        return find_executable("WebKitWebDriver")

    def install_webdriver(self, dest=None, channel=None, browser_binary=None):
        raise NotImplementedError

    def version(self, binary=None, webdriver_binary=None):
        if binary is None:
            return None
        output = call(binary, "--version")
        if output:
            # Stable release output looks like: "Web 3.30.2"
            # Tech Preview output looks like "Web 3.31.3-88-g97db4f40f"
            return output.split()[1]
        return None
