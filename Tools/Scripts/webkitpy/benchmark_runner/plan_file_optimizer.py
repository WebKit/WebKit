import json
import logging
from argparse import ArgumentParser
from collections import OrderedDict

from github_downloader import GithubDownloadTask

_log = logging.getLogger(__name__)


class PlanPrettyPrintEncoder(json.JSONEncoder):
    def iterencode(self, o, _one_shot=False):
        suppress_new_line = False
        inside_tree_section = False
        for entry in super(PlanPrettyPrintEncoder, self).iterencode(o, _one_shot=_one_shot):
            if entry == '"tree"':
                inside_tree_section = True

            if inside_tree_section and entry.strip() == ']':
                inside_tree_section = False

            if not inside_tree_section:
                yield entry
                continue

            if entry == '{':
                suppress_new_line = True
            if entry == '}':
                suppress_new_line = False
            if not suppress_new_line:
                yield entry
                continue

            entry = entry.strip()
            if entry in [',', ':']:
                yield '{} '.format(entry)
            else:
                yield entry


def optimize_plan_file(file_path):
    with open(file_path) as fp:
        plan = json.load(fp, object_pairs_hook=OrderedDict)

    if 'github_source' not in plan:
        _log.info('Current plan doesn\'t contain \'github_source\' key, nothing to optimize.')
        return
    if 'github_subtree' in plan:
        _log.info('Current plan already has \'github_subtree\' optimization.')
        return

    task = GithubDownloadTask(plan['github_source'])
    subtree = task.subtree

    def drop_keys(dictionary, keys_to_drop):
        for key in keys_to_drop:
            dictionary.pop(key, None)

    drop_keys(subtree, ['sha', 'size'])
    tree_with_ordered_entries = []
    for entry in subtree['tree']:
        drop_keys(entry, ['sha', 'size', 'url'])
        ordered_entry = OrderedDict()
        for key in ['path', 'mode', 'type'] + sorted(entry.keys()):
            if key in ordered_entry:
                continue
            if key not in entry:
                continue
            ordered_entry[key] = entry[key]
        tree_with_ordered_entries.append(ordered_entry)
    subtree['tree'] = tree_with_ordered_entries

    plan['github_subtree'] = subtree
    with open(file_path, 'w') as fp:
        json.dump(plan, fp, indent=4, separators=(',', ': '), cls=PlanPrettyPrintEncoder)


def main():
    parser = ArgumentParser('A script that optimize plan file to avoid issuing GitHubAPI request')
    parser.add_argument('--plan', '-p', required=True, help='Path to plan file that needs to be optimized')
    args = parser.parse_args()
    optimize_plan_file(args.plan)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    main()
