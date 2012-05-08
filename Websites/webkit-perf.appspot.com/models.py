#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import hashlib
import json
import math
import re

from datetime import datetime
from datetime import timedelta
from google.appengine.ext import db
from google.appengine.api import memcache
from time import mktime


class NumericIdHolder(db.Model):
    owner = db.ReferenceProperty()
    # Dummy class whose sole purpose is to generate key().id()


def create_in_transaction_with_numeric_id_holder(callback):
    id_holder = NumericIdHolder()
    id_holder.put()
    id_holder = NumericIdHolder.get(id_holder.key())
    owner = None
    try:
        owner = db.run_in_transaction(callback, id_holder.key().id())
        if owner:
            id_holder.owner = owner
            id_holder.put()
    finally:
        if not owner:
            id_holder.delete()
    return owner


def delete_model_with_numeric_id_holder(model):
    id_holder = NumericIdHolder.get_by_id(model.id)
    model.delete()
    id_holder.delete()


def model_from_numeric_id(id, expected_kind):
    id_holder = NumericIdHolder.get_by_id(id)
    return id_holder.owner if id_holder and id_holder.owner and isinstance(id_holder.owner, expected_kind) else None


def _create_if_possible(model, key, name):

    def execute(id):
        if model.get_by_key_name(key):
            return None
        branch = model(id=id, name=name, key_name=key)
        branch.put()
        return branch

    return create_in_transaction_with_numeric_id_holder(execute)


class Branch(db.Model):
    id = db.IntegerProperty(required=True)
    name = db.StringProperty(required=True)

    @staticmethod
    def create_if_possible(key, name):
        return _create_if_possible(Branch, key, name)


class Platform(db.Model):
    id = db.IntegerProperty(required=True)
    name = db.StringProperty(required=True)
    hidden = db.BooleanProperty()

    @staticmethod
    def create_if_possible(key, name):
        return _create_if_possible(Platform, key, name)


class Builder(db.Model):
    name = db.StringProperty(required=True)
    password = db.StringProperty(required=True)

    @staticmethod
    def create(name, raw_password):
        return Builder(name=name, password=Builder._hashed_password(raw_password), key_name=name).put()

    def update_password(self, raw_password):
        self.password = Builder._hashed_password(raw_password)
        self.put()

    def authenticate(self, raw_password):
        return self.password == hashlib.sha256(raw_password).hexdigest()

    @staticmethod
    def _hashed_password(raw_password):
        return hashlib.sha256(raw_password).hexdigest()


class Build(db.Model):
    branch = db.ReferenceProperty(Branch, required=True, collection_name='build_branch')
    platform = db.ReferenceProperty(Platform, required=True, collection_name='build_platform')
    builder = db.ReferenceProperty(Builder, required=True, collection_name='builder_key')
    buildNumber = db.IntegerProperty(required=True)
    revision = db.IntegerProperty(required=True)
    chromiumRevision = db.IntegerProperty()
    timestamp = db.DateTimeProperty(required=True)

    @staticmethod
    def get_or_insert_from_log(log):
        builder = log.builder()
        key_name = builder.name + ':' + str(int(mktime(log.timestamp().timetuple())))

        return Build.get_or_insert(key_name, branch=log.branch(), platform=log.platform(), builder=builder,
            buildNumber=log.build_number(), timestamp=log.timestamp(),
            revision=log.webkit_revision(), chromiumRevision=log.chromium_revision())


class Test(db.Model):
    id = db.IntegerProperty(required=True)
    name = db.StringProperty(required=True)
    # FIXME: Storing branches and platforms separately is flawed since a test maybe available on
    # one platform but only on some branch and vice versa.
    branches = db.ListProperty(db.Key)
    platforms = db.ListProperty(db.Key)
    unit = db.StringProperty()
    hidden = db.BooleanProperty()

    @staticmethod
    def update_or_insert(test_name, branch, platform, unit=None):
        existing_test = [None]

        def execute(id):
            test = Test.get_by_key_name(test_name)
            if test:
                if branch.key() not in test.branches:
                    test.branches.append(branch.key())
                if platform.key() not in test.platforms:
                    test.platforms.append(platform.key())
                test.unit = unit
                test.put()
                existing_test[0] = test
                return None

            test = Test(id=id, name=test_name, key_name=test_name, unit=unit, branches=[branch.key()], platforms=[platform.key()])
            test.put()
            return test

        return create_in_transaction_with_numeric_id_holder(execute) or existing_test[0]

    def merge(self, other):
        assert self.key() != other.key()

        merged_results = TestResult.all()
        merged_results.filter('name =', other.name)

        # FIXME: We should be doing this check in a transaction but only ancestor queries are allowed
        for result in merged_results:
            if TestResult.get_by_key_name(TestResult.key_name(result.build, self.name)):
                return None

        branches_and_platforms_to_update = set()
        for result in merged_results:
            branches_and_platforms_to_update.add((result.build.branch.id, result.build.platform.id))
            result.replace_to_change_test_name(self.name)

        delete_model_with_numeric_id_holder(other)

        return branches_and_platforms_to_update


class TestResult(db.Model):
    name = db.StringProperty(required=True)
    build = db.ReferenceProperty(Build, required=True)
    value = db.FloatProperty(required=True)
    valueMedian = db.FloatProperty()
    valueStdev = db.FloatProperty()
    valueMin = db.FloatProperty()
    valueMax = db.FloatProperty()

    @staticmethod
    def key_name(build, test_name):
        return build.key().name() + ':' + test_name

    @classmethod
    def get_or_insert_from_parsed_json(cls, test_name, build, result):
        key_name = cls.key_name(build, test_name)

        def _float_or_none(dictionary, key):
            value = dictionary.get(key)
            if value:
                return float(value)
            return None

        if not isinstance(result, dict):
            return cls.get_or_insert(key_name, name=test_name, build=build, value=float(result))

        return cls.get_or_insert(key_name, name=test_name, build=build, value=float(result['avg']),
            valueMedian=_float_or_none(result, 'median'), valueStdev=_float_or_none(result, 'stdev'),
            valueMin=_float_or_none(result, 'min'), valueMax=_float_or_none(result, 'max'))

    def replace_to_change_test_name(self, new_name):
        clone = TestResult(key_name=TestResult.key_name(self.build, new_name), name=new_name, build=self.build,
            value=self.value, valueMedian=self.valueMedian, valueStdev=self.valueMin, valueMin=self.valueMin, valueMax=self.valueMax)
        clone.put()
        self.delete()
        return clone


class ReportLog(db.Model):
    timestamp = db.DateTimeProperty(required=True)
    headers = db.TextProperty()
    payload = db.TextProperty()
    commit = db.BooleanProperty()

    def _parsed_payload(self):
        if self.__dict__.get('_parsed') == None:
            try:
                self._parsed = json.loads(self.payload)
            except ValueError:
                self._parsed = False
        return self._parsed

    def get_value(self, keyName):
        if not self._parsed_payload():
            return None
        return self._parsed.get(keyName)

    def results(self):
        return self.get_value('results')

    def results_are_well_formed(self):

        def _is_float_convertible(value):
            try:
                float(value)
                return True
            except TypeError:
                return False
            except ValueError:
                return False

        if not isinstance(self.results(), dict):
            return False

        for testResult in self.results().values():
            if isinstance(testResult, dict):
                for key, value in testResult.iteritems():
                    if key != "unit" and not _is_float_convertible(value):
                        return False
                if 'avg' not in testResult:
                    return False
                continue
            if not _is_float_convertible(testResult):
                return False

        return True

    def builder(self):
        return self._model_by_key_name_in_payload(Builder, 'builder-name')

    def branch(self):
        return self._model_by_key_name_in_payload(Branch, 'branch')

    def platform(self):
        return self._model_by_key_name_in_payload(Platform, 'platform')

    def build_number(self):
        return self._integer_in_payload('build-number')

    def webkit_revision(self):
        return self._integer_in_payload('webkit-revision')

    def chromium_revision(self):
        return self._integer_in_payload('chromium-revision')

    def _model_by_key_name_in_payload(self, model, keyName):
        key = self.get_value(keyName)
        if not key:
            return None
        return model.get_by_key_name(key)

    def _integer_in_payload(self, keyName):
        try:
            return int(self.get_value(keyName))
        except TypeError:
            return None
        except ValueError:
            return None

    # FIXME: We also have timestamp as a member variable.
    def timestamp(self):
        try:
            return datetime.fromtimestamp(self._integer_in_payload('timestamp'))
        except TypeError:
            return None
        except ValueError:
            return None


class PersistentCache(db.Model):
    value = db.TextProperty(required=True)

    @staticmethod
    def set_cache(name, value):
        memcache.set(name, value)
        PersistentCache(key_name=name, value=value).put()

    @staticmethod
    def get_cache(name):
        value = memcache.get(name)
        if value:
            return value
        cache = PersistentCache.get_by_key_name(name)
        if not cache:
            return None
        memcache.set(name, cache.value)
        return cache.value


class Runs(db.Model):
    branch = db.ReferenceProperty(Branch, required=True, collection_name='runs_branch')
    platform = db.ReferenceProperty(Platform, required=True, collection_name='runs_platform')
    test = db.ReferenceProperty(Test, required=True, collection_name='runs_test')
    json_runs = db.TextProperty()
    json_averages = db.TextProperty()
    json_min = db.FloatProperty()
    json_max = db.FloatProperty()

    @staticmethod
    def _generate_runs(branch, platform, test_name):
        builds = Build.all()
        builds.filter('branch =', branch)
        builds.filter('platform =', platform)

        for build in builds:
            results = TestResult.all()
            results.filter('name =', test_name)
            results.filter('build =', build)
            for result in results:
                yield build, result
        raise StopIteration

    @staticmethod
    def _entry_from_build_and_result(build, result):
        builder_id = build.builder.key().id()
        timestamp = mktime(build.timestamp.timetuple())
        statistics = None
        supplementary_revisions = None

        if result.valueStdev != None and result.valueMin != None and result.valueMax != None:
            statistics = {'stdev': result.valueStdev, 'min': result.valueMin, 'max': result.valueMax}

        if build.chromiumRevision != None:
            supplementary_revisions = {'Chromium': build.chromiumRevision}

        return [result.key().id(),
            [build.key().id(), build.buildNumber, build.revision, supplementary_revisions],
            timestamp, result.value, 0,  # runNumber
            [],  # annotations
            builder_id, statistics]

    @staticmethod
    def _timestamp_and_value_from_json_entry(json_entry):
        return json_entry[2], json_entry[3]

    @staticmethod
    def _key_name(branch_id, platform_id, test_id):
        return 'runs:%d,%d,%d' % (test_id, branch_id, platform_id)

    @classmethod
    def update_or_insert(cls, branch, platform, test):
        key_name = cls._key_name(branch.id, platform.id, test.id)
        runs = Runs(key_name=key_name, branch=branch, platform=platform, test=test, json_runs='', json_averages='')

        for build, result in cls._generate_runs(branch, platform, test.name):
            runs.update_incrementally(build, result, check_duplicates_and_save=False)

        runs.put()
        memcache.set(key_name, runs.to_json())
        return runs

    def update_incrementally(self, build, result, check_duplicates_and_save=True):
        new_entry = Runs._entry_from_build_and_result(build, result)

        # Check for duplicate entries
        if check_duplicates_and_save:
            revision_is_in_runs = str(build.revision) in json.loads('{' + self.json_averages + '}')
            if revision_is_in_runs and new_entry[1] in [entry[1] for entry in json.loads('[' + self.json_runs + ']')]:
                return

        if self.json_runs:
            self.json_runs += ','

        if self.json_averages:
            self.json_averages += ','

        self.json_runs += json.dumps(new_entry)
        # FIXME: Calculate the average. In practice, we wouldn't have more than one value for a given revision.
        self.json_averages += '"%d": %f' % (build.revision, result.value)
        self.json_min = min(self.json_min, result.value) if self.json_min != None else result.value
        self.json_max = max(self.json_max, result.value)

        if check_duplicates_and_save:
            self.put()
            memcache.set(self.key().name(), self.to_json())

    @staticmethod
    def get_by_objects(branch, platform, test):
        return Runs.get_by_key_name(Runs._key_name(branch.id, platform.id, test.id))

    @classmethod
    def json_by_ids(cls, branch_id, platform_id, test_id):
        key_name = cls._key_name(branch_id, platform_id, test_id)
        runs_json = memcache.get(key_name)
        if not runs_json:
            runs = cls.get_by_key_name(key_name)
            if not runs:
                return None
            runs_json = runs.to_json()
            memcache.set(key_name, runs_json)
        return runs_json

    def to_json(self):
        # date_range is never used by common.js.
        return '{"test_runs": [%s], "averages": {%s}, "min": %s, "max": %s, "unit": %s, "date_range": null, "stat": "ok"}' % (self.json_runs,
            self.json_averages, str(self.json_min) if self.json_min else 'null', str(self.json_max) if self.json_max else 'null',
            '"%s"' % self.test.unit if self.test.unit else 'null')

    def chart_params(self, display_days, now=datetime.utcnow().replace(hour=12, minute=0, second=0, microsecond=0)):
        chart_data_x = []
        chart_data_y = []
        end_time = now
        start_timestamp = mktime((end_time - timedelta(display_days)).timetuple())
        end_timestamp = mktime(end_time.timetuple())

        for entry in json.loads('[' + self.json_runs + ']'):
            timestamp, value = Runs._timestamp_and_value_from_json_entry(entry)
            if timestamp < start_timestamp or timestamp > end_timestamp:
                continue
            chart_data_x.append(timestamp)
            chart_data_y.append(value)

        if not chart_data_y:
            return None

        dates = [end_time - timedelta(display_days / 7.0 * (7 - i)) for i in range(0, 8)]

        y_max = max(chart_data_y) * 1.1
        y_axis_label_step = int(y_max / 5 + 0.5)  # This won't work for decimal numbers

        return {
            'cht': 'lxy',  # Specify with X and Y coordinates
            'chxt': 'x,y',  # Display both X and Y axies
            'chxl': '0:|' + '|'.join([date.strftime('%b %d') for date in dates]),  # X-axis labels
            'chxr': '1,0,%f,%f' % (int(y_max + 0.5), y_axis_label_step),  # Y-axis range: min=0, max, step
            'chds': '%f,%f,%f,%f' % (start_timestamp, end_timestamp, 0, y_max),  # X, Y data range
            'chxs': '1,676767,11.167,0,l,676767',  # Y-axis label: 1,color,font-size,centerd on tick,axis line/no ticks, tick color
            'chs': '360x240',  # Image size: 360px by 240px
            'chco': 'ff0000',  # Plot line color
            'chg': '%f,20,0,0' % (100 / (len(dates) - 1)),  # X, Y grid line step sizes - max is 100.
            'chls': '3',  # Line thickness
            'chf': 'bg,s,eff6fd',  # Transparent background
            'chd': 't:' + ','.join([str(x) for x in chart_data_x]) + '|' + ','.join([str(y) for y in chart_data_y]),  # X, Y data
        }


class DashboardImage(db.Model):
    image = db.BlobProperty(required=True)
    createdAt = db.DateTimeProperty(required=True, auto_now=True)

    @staticmethod
    def create(branch_id, platform_id, test_id, display_days, image):
        key_name = DashboardImage.key_name(branch_id, platform_id, test_id, display_days)
        instance = DashboardImage(key_name=key_name, image=image)
        instance.put()
        memcache.set('dashboard-image:' + key_name, image)
        return instance

    @staticmethod
    def get_image(branch_id, platform_id, test_id, display_days):
        key_name = DashboardImage.key_name(branch_id, platform_id, test_id, display_days)
        image = memcache.get('dashboard-image:' + key_name)
        if not image:
            instance = DashboardImage.get_by_key_name(key_name)
            image = instance.image
            memcache.set('dashboard-image:' + key_name, image)
        return image

    @classmethod
    def needs_update(cls, branch_id, platform_id, test_id, display_days, now=datetime.now()):
        if display_days < 10:
            return True
        image = DashboardImage.get_by_key_name(cls.key_name(branch_id, platform_id, test_id, display_days))
        duration = math.sqrt(display_days) / 10
        # e.g. 13 hours for 30 days, 23 hours for 90 days, and 46 hours for 365 days
        return not image or image.createdAt < now - timedelta(duration)

    @staticmethod
    def key_name(branch_id, platform_id, test_id, display_days):
        return '%d:%d:%d:%d' % (branch_id, platform_id, test_id, display_days)
