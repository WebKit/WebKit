import subprocess
import re

"""
Minimally wraps CoreSimulator functionality through simctl.

If possible, use real CoreSimulator.framework functionality by linking to the framework itself.
Do not use PyObjC to dlopen the framework.
"""


def get_runtimes(only_available=True):
    """
    Give a dictionary mapping
    :return: A dictionary mapping iOS version string to runtime identifier.
    :rtype: dict
    """
    runtimes = {}
    runtime_re = re.compile(b'iOS (?P<version>[0-9]+\.[0-9]) \([0-9]+\.[0-9]+ - (?P<update>[^)]+)\) \((?P<identifier>[^)]+)\)( \((?P<unavailable>[^)]+)\))?')
    stdout = subprocess.check_output(['xcrun', '-sdk', 'iphonesimulator', 'simctl', 'list', 'runtimes'])
    lines = iter(stdout.splitlines())
    header = next(lines)
    if header != '== Runtimes ==':
        return None

    for line in lines:
        runtime_match = runtime_re.match(line)
        if not runtime_match:
            continue
        runtime = runtime_match.groupdict()
        version = tuple([int(component) for component in runtime_match.group('version').split('.')])
        runtime = {
            'identifier': runtime['identifier'],
            'available': runtime['unavailable'] is None,
            'version': version,
        }
        if only_available and not runtime['available']:
            continue

        runtimes[version] = runtime

    return runtimes


def get_devices():
    """
    :return: A dictionary mapping iOS version to device hardware model, simulator UDID, and state.
    :rtype: dict
    """
    devices = {}
    version_re = re.compile('-- iOS (?P<version>[0-9]+\.[0-9]+) --')
    devices_re = re.compile('\s*(?P<name>[^(]+ )\((?P<udid>[^)]+)\) \((?P<state>[^)]+)\)')
    stdout = subprocess.check_output(['xcrun', '-sdk', 'iphonesimulator', 'simctl', 'list', 'devices'])
    lines = iter(stdout.splitlines())
    header = next(lines)
    version = None
    if header != '== Devices ==':
        return None

    for line in lines:
        version_match = version_re.match(line)
        if version_match:
            version = tuple([int(component) for component in version_match.group('version').split('.')])
            continue
        device_match = devices_re.match(line)
        if not device_match:
            raise RuntimeError()
        device = device_match.groupdict()
        device['name'] = device['name'].rstrip()

        devices[version][device['udid']] = device

    return devices


def get_device_types():
    """
    :return: A dictionary mapping of device name -> identifier
    :rtype: dict
    """
    device_types = {}
    device_type_re = re.compile('(?P<name>[^(]+)\((?P<identifier>[^)]+)\)')
    stdout = subprocess.check_output(['xcrun', '-sdk', 'iphonesimulator', 'simctl', 'list', 'devicetypes'])
    lines = iter(stdout.splitlines())
    header = next(lines)
    if header != '== Device Types ==':
        return None

    for line in lines:
        device_type_match = device_type_re.match(line)
        if not device_type_match:
            continue
        device_type = device_type_match.groupdict()
        device_type['name'] = device_type['name'].rstrip()
        device_types[device_type['name']] = device_type['identifier']

    return device_types


def get_latest_runtime():
    runtimes = get_runtimes()
    if not runtimes:
        return None
    latest_version = sorted(runtimes.keys())[0]
    return runtimes[latest_version]
