#!/usr/bin/env python3
"""Meant to be run from inside python-test-runner container,
where this track repo is mounted at /python
"""
import argparse
from functools import wraps
from itertools import zip_longest
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import List
from data import Config, ExerciseConfig, ExerciseInfo, ExerciseStatus

# Allow high-performance tests to be skipped
ALLOW_SKIP = ['alphametics', 'largest-series-product']

TEST_RUNNER_DIR = Path('/opt/test-runner')

RUNNERS = {}


def runner(name):
    def _decorator(runner_func):
        RUNNERS[name] = runner_func
        @wraps(runner_func)
        def _wrapper(exercise: ExerciseInfo, workdir: Path, quiet: bool = False):
            return runner_func(exercise, workdir, quiet=quiet)
        return _wrapper
    return _decorator


def copy_file(src: Path, dst: Path, strip_skips=False):
    if strip_skips:
        with src.open('r') as src_file:
            lines = [line for line in src_file.readlines()
                        if not line.strip().startswith('@unittest.skip')]
        with dst.open('w') as dst_file:
            dst_file.writelines(lines)
    else:
        shutil.copy2(src, dst)

def copy_solution_files(exercise: ExerciseInfo, workdir: Path, exercise_config: ExerciseConfig = None):
    if exercise_config is not None:
        solution_files = exercise_config.files.solution
        exemplar_files = exercise_config.files.exemplar
        helper_files = exercise_config.files.editor
    else:
        solution_files = []
        exemplar_files = []
        helper_files = []

    if helper_files:
        helper_files = [exercise.path / h for h in helper_files]
        for helper_file in helper_files:
            dst = workdir / helper_file.relative_to(exercise.path)
            copy_file(helper_file, dst)

    if not solution_files:
        solution_files.append(exercise.solution_stub.name)
    solution_files = [exercise.path / s for s in solution_files]
    if not exemplar_files:
        exemplar_files.append(exercise.exemplar_file.relative_to(exercise.path))
    exemplar_files = [exercise.path / e for e in exemplar_files]

    for solution_file, exemplar_file in zip_longest(solution_files, exemplar_files):
        if solution_file is None:
            copy_file(exemplar_file, workdir / exemplar_file.name)
        elif exemplar_file is None:
            copy_file(solution_file, workdir / solution_file.name)
        else:
            dst = workdir / solution_file.relative_to(exercise.path)
            copy_file(exemplar_file, dst)


def copy_test_files(exercise: ExerciseInfo, workdir: Path, exercise_config = None):
    if exercise_config is not None:
        test_files = exercise_config.files.test
        helper_files = exercise_config.files.editor
    else:
        test_files = []
        helper_files = []

    if helper_files:
        for helper_file_name in helper_files:
            helper_file = exercise.path / helper_file_name
            helper_file_out = workdir / helper_file_name
            copy_file(helper_file, helper_file_out, strip_skips=(exercise.slug not in ALLOW_SKIP))

    if not test_files:
        test_files.append(exercise.test_file.name)

    for test_file_name in test_files:
        test_file = exercise.path / test_file_name
        test_file_out = workdir / test_file_name
        copy_file(test_file, test_file_out, strip_skips=(exercise.slug not in ALLOW_SKIP))


def copy_exercise_files(exercise: ExerciseInfo, workdir: Path):
    exercise_config = None
    if exercise.config_file.is_file():
        workdir_meta = workdir / '.meta'
        workdir_meta.mkdir(exist_ok=True)
        copy_file(exercise.config_file, workdir_meta / exercise.config_file.name)
        exercise_config = exercise.load_config()
    copy_solution_files(exercise, workdir, exercise_config)
    copy_test_files(exercise, workdir, exercise_config)


@runner('pytest')
def run_with_pytest(_exercise, workdir, quiet: bool = False) -> int:
    kwargs = {'cwd': str(workdir)}
    if quiet:
        kwargs['stdout'] = subprocess.DEVNULL
        kwargs['stderr'] = subprocess.DEVNULL
    return subprocess.run([sys.executable, '-m', 'pytest'], **kwargs).returncode


@runner('test-runner')
def run_with_test_runner(exercise, workdir, quiet: bool = False) -> int:
    kwargs = {}
    if quiet:
        kwargs['stdout'] = subprocess.DEVNULL
        kwargs['stderr'] = subprocess.DEVNULL
    if TEST_RUNNER_DIR.is_dir():
        kwargs['cwd'] = str(TEST_RUNNER_DIR)
        args = ['./bin/run.sh', exercise.slug, workdir, workdir]
    else:
        args = [
            'docker-compose',
            'run',
            '-w', str(TEST_RUNNER_DIR),
            '--entrypoint', './bin/run.sh',
            '-v', f'{workdir}:/{exercise.slug}',
            'test-runner',
            exercise.slug,
            f'/{exercise.slug}',
            f'/{exercise.slug}',
        ]
    subprocess.run(args, **kwargs)
    results_file = workdir / 'results.json'
    if results_file.is_file():
        with results_file.open() as f:
            results = json.load(f)
        if results['status'] == 'pass':
            return 0
    return 1


def check_assignment(exercise: ExerciseInfo, runner: str = 'pytest', quiet: bool = False) -> int:
    ret = 1
    with tempfile.TemporaryDirectory(exercise.slug) as workdir:
        workdir = Path(workdir)
        copy_exercise_files(exercise, workdir)
        ret = RUNNERS[runner](exercise, workdir, quiet=quiet)
    return ret


def get_cli() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    runners = list(RUNNERS.keys())
    if not runners:
        print('No runners registered!')
        raise SystemExit(1)
    parser.add_argument('-q', '--quiet', action='store_true')
    parser.add_argument('--deprecated', action='store_true', help='include deprecated exercises', dest='include_deprecated')
    parser.add_argument('--wip', action='store_true', help='include WIP exercises', dest='include_wip')
    parser.add_argument('-r', '--runner', choices=runners, default=runners[0])
    parser.add_argument('exercises', nargs='*')
    return parser


def main():
    opts = get_cli().parse_args()
    config = Config.load()
    status_filter = {ExerciseStatus.Active, ExerciseStatus.Beta}
    if opts.include_deprecated:
        status_filter.add(ExerciseStatus.Deprecated)
    if opts.include_wip:
        status_filter.add(ExerciseStatus.WIP)
    exercises = config.exercises.all(status_filter)
    if opts.exercises:
        # test specific exercises
        exercises = [
            e for e in exercises if e.slug in opts.exercises
        ]
        not_found = [
            slug for slug in opts.exercises
            if not any(e.slug == slug for e in exercises)
        ]
        if not_found:
            for slug in not_found:
                if slug not in exercises:
                    print(f"unknown or disabled exercise '{slug}'")
            raise SystemExit(1)

    print(f'TestEnvironment: {sys.executable.capitalize()}')
    print(f'Runner: {opts.runner}\n\n')

    failures = []
    for exercise in exercises:
        print('# ', exercise.slug)
        if not exercise.test_file:
            print('FAIL: File with test cases not found')
            failures.append('{} (FileNotFound)'.format(exercise.slug))
        else:
            if check_assignment(exercise, runner=opts.runner, quiet=opts.quiet):
                failures.append('{} (TestFailed)'.format(exercise.slug))
        print('')

    if failures:
        print('FAILURES: ', ', '.join(failures))
        raise SystemExit(1)
    else:
        print('SUCCESS!')


if __name__ == "__main__":
    main()
