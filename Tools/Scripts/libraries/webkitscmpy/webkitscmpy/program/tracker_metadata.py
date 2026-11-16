# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import sys

from .command import Command

from webkitbugspy import Tracker


class TrackerMetadata(Command):
    name = 'tracker-metadata'
    aliases = []
    help = 'Fetch and display metadata from the configured bug tracker'

    PROPERTIES = [
        'products', 'components', 'component_descriptions',
        'keywords', 'keyword_descriptions',
    ]

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--format', dest='output_format', type=str, default='json',
            choices=['json', 'text'],
            help='Output format (default: json)',
        )
        parser.add_argument(
            '-p', '--property', dest='properties', action='append', default=None,
            help='Property to display (can be specified multiple times). '
                 'Available: {}'.format(', '.join(cls.PROPERTIES)),
        )
        parser.add_argument(
            '--project', dest='project', type=str, default=None,
            help='Filter components and component descriptions to a specific project',
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        tracker = Tracker.instance()
        if not tracker:
            sys.stderr.write("No bug tracker configured\n")
            return 1

        properties = args.properties or cls.PROPERTIES

        for prop in properties:
            if prop not in cls.PROPERTIES:
                sys.stderr.write("Unknown property '{}'. Available: {}\n".format(
                    prop, ', '.join(cls.PROPERTIES)))
                return 1

        result = cls.get_tracker_metadata(tracker, properties, args.project)
        cls.output_result(result, args.output_format)
        return 0

    @classmethod
    def get_tracker_metadata(cls, tracker, properties, project=None):
        result = {}
        projects = tracker.projects

        if 'products' in properties:
            result['products'] = sorted(projects.keys())

        if 'components' in properties:
            components = {}
            for name, details in sorted(projects.items()):
                if project and name != project:
                    continue
                components[name] = sorted(details['components'].keys())
            result['components'] = components

        if 'component_descriptions' in properties:
            component_descriptions = {}
            for name, details in sorted(projects.items()):
                if project and name != project:
                    continue
                component_descriptions[name] = {
                    comp: info['description']
                    for comp, info in sorted(details['components'].items())
                }
            result['component_descriptions'] = component_descriptions

        if 'keywords' in properties or 'keyword_descriptions' in properties:
            if hasattr(tracker, 'valid_keywords'):
                keywords = tracker.valid_keywords()
            else:
                keywords = {}
            if 'keywords' in properties:
                result['keywords'] = sorted(keywords.keys())
            if 'keyword_descriptions' in properties:
                result['keyword_descriptions'] = dict(sorted(keywords.items()))

        return result

    @classmethod
    def output_result(cls, result, output_format):
        if output_format == 'json':
            print(json.dumps(result, indent=2, default=str))
        else:
            if 'products' in result:
                print("products: {}".format(', '.join(result['products'])))
            if 'components' in result:
                for product, components in sorted(result['components'].items()):
                    print("\ncomponents ({}):".format(product))
                    for comp in components:
                        print("  {}".format(comp))
            if 'component_descriptions' in result:
                for product, descs in sorted(result['component_descriptions'].items()):
                    print("\ncomponent_descriptions ({}):".format(product))
                    for comp, desc in sorted(descs.items()):
                        print("  {}: {}".format(comp, desc))
            if 'keywords' in result:
                print("\nkeywords: {}".format(', '.join(result['keywords'])))
            if 'keyword_descriptions' in result:
                print("\nkeyword_descriptions:")
                for kw, desc in sorted(result['keyword_descriptions'].items()):
                    print("  {}: {}".format(kw, desc))
