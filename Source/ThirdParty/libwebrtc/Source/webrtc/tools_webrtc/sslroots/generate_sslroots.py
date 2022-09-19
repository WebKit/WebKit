#!/usr/bin/env vpython3

# -*- coding:utf-8 -*-
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""This is a tool to transform a crt file into a C/C++ header.

Usage:
python3 generate_sslroots.py certfile.pem [--verbose | -v] [--full_cert | -f]

Arguments:
  -v  Print output while running.
  -f  Add public key and certificate name.  Default is to skip and reduce
      generated file size.

The supported cert files are:
  - Google: https://pki.goog/roots.pem
  - Mozilla: https://curl.se/docs/caextract.html
"""

import subprocess
from optparse import OptionParser
import os
import re

_GENERATED_FILE = 'ssl_roots.h'
_PREFIX = '__generated__'
_EXTENSION = '.crt'
_SUBJECT_NAME_ARRAY = 'subject_name'
_SUBJECT_NAME_VARIABLE = 'SubjectName'
_PUBLIC_KEY_ARRAY = 'public_key'
_PUBLIC_KEY_VARIABLE = 'PublicKey'
_CERTIFICATE_ARRAY = 'certificate'
_CERTIFICATE_VARIABLE = 'Certificate'
_CERTIFICATE_SIZE_VARIABLE = 'CertificateSize'
_INT_TYPE = 'size_t'
_CHAR_TYPE = 'unsigned char* const'
_VERBOSE = 'verbose'
_MOZILLA_BUNDLE_CHECK = '## Certificate data from Mozilla as of:'


def main():
  """The main entrypoint."""
  parser = OptionParser('usage %prog FILE')
  parser.add_option('-v', '--verbose', dest='verbose', action='store_true')
  parser.add_option('-f', '--full_cert', dest='full_cert', action='store_true')
  options, args = parser.parse_args()
  if len(args) < 1:
    parser.error('No crt file specified.')
    return
  root_dir, bundle_type = _SplitCrt(args[0], options)
  _GenCFiles(root_dir, options, bundle_type)
  _Cleanup(root_dir)


def _SplitCrt(source_file, options):
  sub_file_blocks = []
  label_name = ''
  prev_line = None
  root_dir = os.path.dirname(os.path.abspath(source_file)) + '/'
  _PrintOutput(root_dir, options)
  lines = None
  with open(source_file) as f:
    lines = f.readlines()
  mozilla_bundle = any(l.startswith(_MOZILLA_BUNDLE_CHECK) for l in lines)
  for line in lines:
    if line.startswith('#'):
      if mozilla_bundle:
        continue
      if line.startswith('# Label: '):
        sub_file_blocks.append(line)
        label = re.search(r'\".*\"', line)
        temp_label = label.group(0)
        end = len(temp_label) - 1
        label_name = _SafeName(temp_label[1:end])
    if mozilla_bundle and line.startswith('==='):
      sub_file_blocks.append(line)
      label_name = _SafeName(prev_line)
    elif line.startswith('-----END CERTIFICATE-----'):
      sub_file_blocks.append(line)
      new_file_name = root_dir + _PREFIX + label_name + _EXTENSION
      _PrintOutput('Generating: ' + new_file_name, options)
      new_file = open(new_file_name, 'w')
      for out_line in sub_file_blocks:
        new_file.write(out_line)
      new_file.close()
      sub_file_blocks = []
    else:
      sub_file_blocks.append(line)
    prev_line = line
  return root_dir, 'Mozilla' if mozilla_bundle else 'Google'


def _GenCFiles(root_dir, options, bundle_type):
  output_header_file = open(root_dir + _GENERATED_FILE, 'w')
  output_header_file.write(_CreateOutputHeader(bundle_type))
  if options.full_cert:
    subject_name_list = _CreateArraySectionHeader(_SUBJECT_NAME_VARIABLE,
                                                  _CHAR_TYPE, options)
    public_key_list = _CreateArraySectionHeader(_PUBLIC_KEY_VARIABLE,
                                                _CHAR_TYPE, options)
  certificate_list = _CreateArraySectionHeader(_CERTIFICATE_VARIABLE,
                                               _CHAR_TYPE, options)
  certificate_size_list = _CreateArraySectionHeader(_CERTIFICATE_SIZE_VARIABLE,
                                                    _INT_TYPE, options)

  for _, _, files in os.walk(root_dir):
    for current_file in files:
      if current_file.startswith(_PREFIX):
        prefix_length = len(_PREFIX)
        length = len(current_file) - len(_EXTENSION)
        label = current_file[prefix_length:length]
        filtered_output, cert_size = _CreateCertSection(root_dir, current_file,
                                                        label, options)
        output_header_file.write(filtered_output + '\n\n\n')
        if options.full_cert:
          subject_name_list += _AddLabelToArray(label, _SUBJECT_NAME_ARRAY)
          public_key_list += _AddLabelToArray(label, _PUBLIC_KEY_ARRAY)
        certificate_list += _AddLabelToArray(label, _CERTIFICATE_ARRAY)
        certificate_size_list += ('  %s,\n') % (cert_size)

  if options.full_cert:
    subject_name_list += _CreateArraySectionFooter()
    output_header_file.write(subject_name_list)
    public_key_list += _CreateArraySectionFooter()
    output_header_file.write(public_key_list)
  certificate_list += _CreateArraySectionFooter()
  output_header_file.write(certificate_list)
  certificate_size_list += _CreateArraySectionFooter()
  output_header_file.write(certificate_size_list)
  output_header_file.write(_CreateOutputFooter())
  output_header_file.close()


def _Cleanup(root_dir):
  for f in os.listdir(root_dir):
    if f.startswith(_PREFIX):
      os.remove(root_dir + f)


def _CreateCertSection(root_dir, source_file, label, options):
  command = 'openssl x509 -in %s%s -noout -C' % (root_dir, source_file)
  _PrintOutput(command, options)
  output = subprocess.getstatusoutput(command)[1]
  renamed_output = output.replace('unsigned char XXX_',
                                  'const unsigned char ' + label + '_')
  filtered_output = ''
  cert_block = '^const unsigned char.*?};$'
  prog = re.compile(cert_block, re.IGNORECASE | re.MULTILINE | re.DOTALL)
  if not options.full_cert:
    filtered_output = prog.sub('', renamed_output, count=2)
  else:
    filtered_output = renamed_output

  cert_size_block = r'\d\d\d+'
  prog2 = re.compile(cert_size_block, re.MULTILINE | re.VERBOSE)
  result = prog2.findall(renamed_output)
  cert_size = result[len(result) - 1]

  return filtered_output, cert_size


def _CreateOutputHeader(bundle_type):
  output = ('/*\n'
            ' *  Copyright 2004 The WebRTC Project Authors. All rights '
            'reserved.\n'
            ' *\n'
            ' *  Use of this source code is governed by a BSD-style license\n'
            ' *  that can be found in the LICENSE file in the root of the '
            'source\n'
            ' *  tree. An additional intellectual property rights grant can be '
            'found\n'
            ' *  in the file PATENTS.  All contributing project authors may\n'
            ' *  be found in the AUTHORS file in the root of the source tree.\n'
            ' */\n\n'
            '#ifndef RTC_BASE_SSL_ROOTS_H_\n'
            '#define RTC_BASE_SSL_ROOTS_H_\n\n'
            '// This file is the root certificates in C form.\n\n'
            '// It was generated with the following script:\n'
            '// tools_webrtc/sslroots/generate_sslroots.py'
            ' %s_CA_bundle.pem\n\n'
            '// clang-format off\n'
            '// Don\'t bother formatting generated code,\n'
            '// also it would breaks subject/issuer lines.\n\n' % bundle_type)
  return output


def _CreateOutputFooter():
  return '// clang-format on\n\n#endif  // RTC_BASE_SSL_ROOTS_H_\n'


def _CreateArraySectionHeader(type_name, type_type, options):
  output = ('const %s kSSLCert%sList[] = {\n') % (type_type, type_name)
  _PrintOutput(output, options)
  return output


def _AddLabelToArray(label, type_name):
  return ' %s_%s,\n' % (label, type_name)


def _CreateArraySectionFooter():
  return '};\n\n'


def _SafeName(original_file_name):
  bad_chars = ' -./\\()áéíőú\r\n'
  replacement_chars = ''
  for _ in bad_chars:
    replacement_chars += '_'
  translation_table = str.maketrans(bad_chars, replacement_chars)
  return original_file_name.translate(translation_table)


def _PrintOutput(output, options):
  if options.verbose:
    print(output)


if __name__ == '__main__':
  main()
