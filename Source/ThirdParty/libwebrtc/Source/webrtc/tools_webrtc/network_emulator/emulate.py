#!/usr/bin/env vpython3

#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
"""Script for constraining traffic on the local machine."""

import logging
import optparse
import socket
import sys

import config
import network_emulator

_DEFAULT_LOG_LEVEL = logging.INFO

# Default port range to apply network constraints on.
_DEFAULT_PORT_RANGE = (32768, 65535)

# The numbers below are gathered from Google stats from the presets of the Apple
# developer tool called Network Link Conditioner.
_PRESETS = [
    config.ConnectionConfig(1, 'Generic, Bad', 95, 95, 250, 2, 100),
    config.ConnectionConfig(2, 'Generic, Average', 375, 375, 145, 0.1, 100),
    config.ConnectionConfig(3, 'Generic, Good', 1000, 1000, 35, 0, 100),
    config.ConnectionConfig(4, '3G, Average Case', 780, 330, 100, 0, 100),
    config.ConnectionConfig(5, '3G, Good', 850, 420, 90, 0, 100),
    config.ConnectionConfig(6, '3G, Lossy Network', 780, 330, 100, 1, 100),
    config.ConnectionConfig(7, 'Cable Modem', 6000, 1000, 2, 0, 10),
    config.ConnectionConfig(8, 'DSL', 2000, 256, 5, 0, 10),
    config.ConnectionConfig(9, 'Edge, Average Case', 240, 200, 400, 0, 100),
    config.ConnectionConfig(10, 'Edge, Good', 250, 200, 350, 0, 100),
    config.ConnectionConfig(11, 'Edge, Lossy Network', 240, 200, 400, 1, 100),
    config.ConnectionConfig(12, 'Wifi, Average Case', 40000, 33000, 1, 0, 100),
    config.ConnectionConfig(13, 'Wifi, Good', 45000, 40000, 1, 0, 100),
    config.ConnectionConfig(14, 'Wifi, Lossy', 40000, 33000, 1, 0, 100),
]
_PRESETS_DICT = dict((p.num, p) for p in _PRESETS)

_DEFAULT_PRESET_ID = 2
_DEFAULT_PRESET = _PRESETS_DICT[_DEFAULT_PRESET_ID]


class NonStrippingEpilogOptionParser(optparse.OptionParser):
  """Custom parser to let us show the epilog without weird line breaking."""

  def format_epilog(self, formatter):
    return self.epilog


def _GetExternalIp():
  """Finds out the machine's external IP by connecting to google.com."""
  external_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  external_socket.connect(('google.com', 80))
  return external_socket.getsockname()[0]


def _ParseArgs():
  """Define and parse the command-line arguments."""
  presets_string = '\n'.join(str(p) for p in _PRESETS)
  parser = NonStrippingEpilogOptionParser(epilog=(
      '\nAvailable presets:\n'
      '                              Bandwidth (kbps)                  Packet\n'
      'ID Name                       Receive     Send    Queue  Delay   loss \n'
      '-- ----                      ---------   -------- ----- ------- ------\n'
      '%s\n' % presets_string))
  parser.add_option('-p',
                    '--preset',
                    type='int',
                    default=_DEFAULT_PRESET_ID,
                    help=('ConnectionConfig configuration, specified by ID. '
                          'Default: %default'))
  parser.add_option('-r',
                    '--receive-bw',
                    type='int',
                    default=_DEFAULT_PRESET.receive_bw_kbps,
                    help=('Receive bandwidth in kilobit/s. Default: %default'))
  parser.add_option('-s',
                    '--send-bw',
                    type='int',
                    default=_DEFAULT_PRESET.send_bw_kbps,
                    help=('Send bandwidth in kilobit/s. Default: %default'))
  parser.add_option('-d',
                    '--delay',
                    type='int',
                    default=_DEFAULT_PRESET.delay_ms,
                    help=('Delay in ms. Default: %default'))
  parser.add_option('-l',
                    '--packet-loss',
                    type='float',
                    default=_DEFAULT_PRESET.packet_loss_percent,
                    help=('Packet loss in %. Default: %default'))
  parser.add_option('-q',
                    '--queue',
                    type='int',
                    default=_DEFAULT_PRESET.queue_slots,
                    help=('Queue size as number of slots. Default: %default'))
  parser.add_option('--port-range',
                    default='%s,%s' % _DEFAULT_PORT_RANGE,
                    help=('Range of ports for constrained network. Specify as '
                          'two comma separated integers. Default: %default'))
  parser.add_option('--target-ip',
                    default=None,
                    help=('The interface IP address to apply the rules for. '
                          'Default: the external facing interface IP address.'))
  parser.add_option('-v',
                    '--verbose',
                    action='store_true',
                    default=False,
                    help=('Turn on verbose output. Will print all \'ipfw\' '
                          'commands that are executed.'))

  options = parser.parse_args()[0]

  # Find preset by ID, if specified.
  if options.preset and options.preset not in _PRESETS_DICT:
    parser.error('Invalid preset: %s' % options.preset)

  # Simple validation of the IP address, if supplied.
  if options.target_ip:
    try:
      socket.inet_aton(options.target_ip)
    except socket.error:
      parser.error('Invalid IP address specified: %s' % options.target_ip)

  # Convert port range into the desired tuple format.
  try:
    if isinstance(options.port_range, str):
      options.port_range = tuple(
          int(port) for port in options.port_range.split(','))
      if len(options.port_range) != 2:
        parser.error('Invalid port range specified, please specify two '
                     'integers separated by a comma.')
  except ValueError:
    parser.error('Invalid port range specified.')

  _InitLogging(options.verbose)
  return options


def _InitLogging(verbose):
  """Setup logging."""
  log_level = _DEFAULT_LOG_LEVEL
  if verbose:
    log_level = logging.DEBUG
  logging.basicConfig(level=log_level, format='%(message)s')


def main():
  options = _ParseArgs()

  # Build a configuration object. Override any preset configuration settings if
  # a value of a setting was also given as a flag.
  connection_config = _PRESETS_DICT[options.preset]
  if options.receive_bw is not _DEFAULT_PRESET.receive_bw_kbps:
    connection_config.receive_bw_kbps = options.receive_bw
  if options.send_bw is not _DEFAULT_PRESET.send_bw_kbps:
    connection_config.send_bw_kbps = options.send_bw
  if options.delay is not _DEFAULT_PRESET.delay_ms:
    connection_config.delay_ms = options.delay
  if options.packet_loss is not _DEFAULT_PRESET.packet_loss_percent:
    connection_config.packet_loss_percent = options.packet_loss
  if options.queue is not _DEFAULT_PRESET.queue_slots:
    connection_config.queue_slots = options.queue
  emulator = network_emulator.NetworkEmulator(connection_config,
                                              options.port_range)
  try:
    emulator.CheckPermissions()
  except network_emulator.NetworkEmulatorError as e:
    logging.error('Error: %s\n\nCause: %s', e.fail_msg, e.error)
    return -1

  if not options.target_ip:
    external_ip = _GetExternalIp()
  else:
    external_ip = options.target_ip

  logging.info('Constraining traffic to/from IP: %s', external_ip)
  try:
    emulator.Emulate(external_ip)
    logging.info(
        'Started network emulation with the following configuration:\n'
        '  Receive bandwidth: %s kbps (%s kB/s)\n'
        '  Send bandwidth   : %s kbps (%s kB/s)\n'
        '  Delay            : %s ms\n'
        '  Packet loss      : %s %%\n'
        '  Queue slots      : %s', connection_config.receive_bw_kbps,
        connection_config.receive_bw_kbps / 8, connection_config.send_bw_kbps,
        connection_config.send_bw_kbps / 8, connection_config.delay_ms,
        connection_config.packet_loss_percent, connection_config.queue_slots)
    logging.info('Affected traffic: IP traffic on ports %s-%s',
                 options.port_range[0], options.port_range[1])
    input('Press Enter to abort Network Emulation...')
    logging.info('Flushing all Dummynet rules...')
    network_emulator.Cleanup()
    logging.info('Completed Network Emulation.')
    return 0
  except network_emulator.NetworkEmulatorError as e:
    logging.error('Error: %s\n\nCause: %s', e.fail_msg, e.error)
    return -2


if __name__ == '__main__':
  sys.exit(main())
