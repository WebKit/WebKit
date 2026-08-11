# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Configuration categories, typed enums, and ConfigurationSpecifier for test expectations."""

import enum
import re
from dataclasses import dataclass, field
from typing import FrozenSet, Optional, Set, List


class ConfigurationCategory(enum.Enum):
    """Configuration category — declaration order defines the canonical ordering."""
    PLATFORM = 'platform'
    VERSION = 'version'
    STYLE = 'style'
    HARDWARE = 'hardware'
    ARCHITECTURE = 'architecture'
    FLAVOR = 'flavor'


_CATEGORY_ORDER = {cat: i for i, cat in enumerate(ConfigurationCategory)}


class Platform(enum.Enum):
    MAC = 'mac'
    IOS = 'ios'
    WATCHOS = 'watchos'
    TVOS = 'tvos'
    VISIONOS = 'visionos'
    LINUX = 'linux'
    WIN = 'win'
    GTK = 'gtk'
    WPE = 'wpe'


class BuildType(enum.Enum):
    DEBUG = 'debug'
    RELEASE = 'release'
    PRODUCTION = 'production'
    ASAN = 'asan'
    GUARDMALLOC = 'guardmalloc'


class Architecture(enum.Enum):
    ARM64 = 'arm64'
    X86_64 = 'x86_64'
    X86 = 'x86'
    ARM64_32 = 'arm64_32'
    ARMV7K = 'armv7k'


class Hardware(enum.Enum):
    SIMULATOR = 'simulator'
    DEVICE = 'device'
    IPHONE = 'iphone'
    IPAD = 'ipad'
    VISION = 'vision'
    WATCH = 'watch'
    TV = 'tv'


_PLATFORM_BY_VALUE = {p.value: p for p in Platform}
_BUILD_TYPE_BY_VALUE = {b.value: b for b in BuildType}
_ARCHITECTURE_BY_VALUE = {a.value: a for a in Architecture}
_HARDWARE_BY_VALUE = {h.value: h for h in Hardware}

PLATFORM_TOKENS = frozenset(p.value for p in Platform)
STYLE_TOKENS = frozenset(b.value for b in BuildType)
HARDWARE_TOKENS = frozenset(h.value for h in Hardware)
ARCHITECTURE_TOKENS = frozenset(a.value for a in Architecture)


# Future-proofing escape hatch: any lowercase token ending in "os" (e.g. a
# future platform like "xros") is recognized as a platform without requiring
# the Platform enum to be amended. The enum remains the source of truth for
# enumerated iteration and linter token ordering; this regex only broadens
# classification.
_PLATFORM_SHAPE_RE = re.compile(r'^[a-z]+os$')

CATEGORY_TOKENS = {
    ConfigurationCategory.PLATFORM: PLATFORM_TOKENS,
    ConfigurationCategory.STYLE: STYLE_TOKENS,
    ConfigurationCategory.HARDWARE: HARDWARE_TOKENS,
    ConfigurationCategory.ARCHITECTURE: ARCHITECTURE_TOKENS,
}

CATEGORY_NAMES = {
    ConfigurationCategory.PLATFORM: 'Platform',
    ConfigurationCategory.VERSION: 'Version',
    ConfigurationCategory.STYLE: 'Style',
    ConfigurationCategory.HARDWARE: 'Hardware',
    ConfigurationCategory.ARCHITECTURE: 'Architecture',
    ConfigurationCategory.FLAVOR: 'Flavor',
}


@dataclass(frozen=True)
class ConfigurationSpecifier:
    platforms: FrozenSet[Platform] = field(default_factory=frozenset)
    architectures: FrozenSet[Architecture] = field(default_factory=frozenset)
    build_types: FrozenSet[BuildType] = field(default_factory=frozenset)
    hardware: FrozenSet[Hardware] = field(default_factory=frozenset)
    flavors: FrozenSet[str] = field(default_factory=frozenset)
    version_specifier: Optional[object] = None

    @classmethod
    def from_tokens(cls, tokens: Set[str], version_specifiers=None):
        platforms = frozenset(
            _PLATFORM_BY_VALUE[t] for t in tokens if t in _PLATFORM_BY_VALUE
        )
        build_types = frozenset(
            _BUILD_TYPE_BY_VALUE[t] for t in tokens if t in _BUILD_TYPE_BY_VALUE
        )
        architectures = frozenset(
            _ARCHITECTURE_BY_VALUE[t] for t in tokens if t in _ARCHITECTURE_BY_VALUE
        )
        hardware_set = frozenset(
            _HARDWARE_BY_VALUE[t] for t in tokens if t in _HARDWARE_BY_VALUE
        )
        known = PLATFORM_TOKENS | STYLE_TOKENS | HARDWARE_TOKENS | ARCHITECTURE_TOKENS
        flavors = frozenset(t for t in tokens if t not in known)

        vs = tuple(version_specifiers) if version_specifiers else None

        return cls(
            platforms=platforms,
            architectures=architectures,
            build_types=build_types,
            hardware=hardware_set,
            flavors=flavors,
            version_specifier=vs,
        )

    def matches(self, current_tokens: Set[str],
                current_version: Optional[str] = None,
                version_order: Optional[List[str]] = None) -> bool:
        if self.platforms:
            if not any(p.value in current_tokens for p in self.platforms):
                return False
        if self.build_types:
            if not any(b.value in current_tokens for b in self.build_types):
                return False
        if self.architectures:
            if not any(a.value in current_tokens for a in self.architectures):
                return False
        if self.hardware:
            if not any(h.value in current_tokens for h in self.hardware):
                return False
        if self.flavors:
            if not self.flavors.issubset(current_tokens):
                return False
        if self.version_specifier:
            if not current_version or not version_order:
                return False
            specs = self.version_specifier if isinstance(self.version_specifier, tuple) else (self.version_specifier,)
            for spec in specs:
                if not spec.matches(current_version, version_order):
                    return False
        return True

    def to_tokens(self) -> Set[str]:
        tokens = set()
        for p in self.platforms:
            tokens.add(p.value)
        for b in self.build_types:
            tokens.add(b.value)
        for a in self.architectures:
            tokens.add(a.value)
        for h in self.hardware:
            tokens.add(h.value)
        tokens.update(self.flavors)
        return tokens


def matches_platform(token):
    token_lower = token.lower()
    if token_lower in PLATFORM_TOKENS:
        return True
    if _PLATFORM_SHAPE_RE.match(token_lower):
        return True
    return False


def get_token_category(token, version_tokens=None):
    token_lower = token.lower()

    if token_lower in PLATFORM_TOKENS:
        return ConfigurationCategory.PLATFORM
    if _PLATFORM_SHAPE_RE.match(token_lower):
        return ConfigurationCategory.PLATFORM
    if token_lower in STYLE_TOKENS:
        return ConfigurationCategory.STYLE
    if token_lower in HARDWARE_TOKENS:
        return ConfigurationCategory.HARDWARE
    if token_lower in ARCHITECTURE_TOKENS:
        return ConfigurationCategory.ARCHITECTURE

    if token.endswith('+') or token.endswith('-') or ('-' in token and not token.endswith('-')):
        return ConfigurationCategory.VERSION

    if version_tokens and token_lower in version_tokens:
        return ConfigurationCategory.VERSION

    if token_lower.isidentifier() or re.match(r'^[a-z][a-z0-9_-]*$', token_lower):
        return ConfigurationCategory.FLAVOR

    return None
