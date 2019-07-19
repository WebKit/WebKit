// Copyright (C) 2019 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import {queryToParams} from '/assets/js/common.js';

// These are flipped delibrately, it makes the fromQuery function return configurations in an
// intuitive order.
const REQUIRED_MEMBERS = ['architecture', 'version', 'is_simulator', 'platform'];
const OPTIONAL_MEMBERS = ['flavor', 'style', 'model', 'version_name'];
const FILTERING_MEMBERS = ['sdk'];
const VERSION_OFFSET_CONSTANT = 1000;

// This class is very similar to the Python Configuration class
class Configuration {
    static members() {
        return [...FILTERING_MEMBERS, ...OPTIONAL_MEMBERS, ...REQUIRED_MEMBERS];
    }
    static fromQuery() {
        const params = queryToParams(document.URL.split('?')[1]);
        let configs = [new Configuration()];

        Configuration.members().forEach(member => {
            const argumentsForMember = params[member];
            if (!argumentsForMember || argumentsForMember.length === 0)
                return;

            let originalConfigs = configs;
            configs = [];
            argumentsForMember.forEach(argument => {
                originalConfigs.forEach(config => {
                    let newConfig = new Configuration(config);
                    newConfig[member] = argument;
                    configs.push(new Configuration(newConfig));
                });
            });
        });

        return configs;
    }
    static versionToInteger(version) {
        if (typeof version === 'number')
            return version;
        let versionList = null;
        if (typeof version === 'string')
            versionList = version.split('.');
        else {
            if (!Array.isArray(version))
                return null;
            versionList = version;
        }
        if (versionList.length < 1 || versionList.length > 3)
            return null;

        let result = 0;
        let index = 0;
        while (index < 3) {
            result *= VERSION_OFFSET_CONSTANT;
            if (index < versionList.length)
                result += parseInt(versionList[index]);
            ++index;
        }
        return result;
    }
    static integerToVersion(version) {
        let result = [0, 0, 0];
        for (let index = 0; index < result.length; ++index) {
            result[result.length - (index + 1)] = version % VERSION_OFFSET_CONSTANT;
            version = Math.floor(version / VERSION_OFFSET_CONSTANT);
        }
        return result.join('.');
    }

    constructor(json = {}) {
        this.platform = json.platform ? json.platform : null;
        this.version = json.version ? Configuration.versionToInteger(json.version) : null;
        this.version_name = json.version_name ? json.version_name : null;
        this.sdk = json.sdk ? json.sdk : null;
        if (typeof json.is_simulator === 'string')
            this.is_simulator = json.is_simulator.toLowerCase() === 'true' ? true : false;
        else if (typeof json.is_simulator === 'boolean')
            this.is_simulator = json.is_simulator;
        else
            this.is_simulator = null;
        this.style = json.style ? json.style : null;
        this.flavor = json.flavor ? json.flavor : null;
        this.model = json.model ? json.model : null;
        this.architecture = json.architecture ? json.architecture : null;
    }
    toKey() {
        let result = ''
        if (this.platform != null)
            result += ' ' + this.platform
        if (this.version != null)
            result += ' ' + Configuration.integerToVersion(this.version);
        else if (this.version_name != null)
            result += ' ' + this.version_name;
        if (this.sdk != null)
            result += ' (' + this.sdk + ')';
        if (this.is_simulator != null && this.is_simulator)
            result += ' Simulator';
        if (this.style != null)
            result += ' ' + this.style;
        if (this.flavor != null)
            result += ' ' + this.flavor;
        if (this.model != null)
            result += ' on ' + this.model;
        if (this.architecture != null)
            result += ' using ' + this.architecture;

        if (result)
            return result.substr(1);
        return 'All';
    }
    toString() {
        let result = '';

        // Version name implies platform and version
        if (this.version_name != null)
            result += ' ' + this.version_name;
        else {
            if (this.platform != null)
                result += ' ' + this.platform
            if (this.version != null)
                result += ' ' + Configuration.integerToVersion(this.version);
        }
        if (this.is_simulator != null && this.is_simulator)
            result += ' Simulator';
        if (this.flavor != null)
            result += ' ' + this.flavor;
        if (this.style != null) {
            result += ' ' + this.style.split('-').map(word => word.charAt(0).toUpperCase() + word.slice(1)).join('-');
        } if (this.model != null)
            result += ' on ' + this.model;

        if (this.sdk != null)
            result += ' (' + this.sdk + ')';

        if (result)
            return result.substr(1);
        return 'All';
    }
    compare(configuration) {
        if (configuration === null)
            return 1;

        const lst = ['platform', 'is_simulator', 'version', 'version_name', 'flavor', 'style', 'architecture', 'model'];
        let result = 0;
        lst.forEach(key => {
            if (result != 0)
                return;

            if (this[key] === null && configuration[key] !== null)
                result = -1;
            else if (this[key] !== null && configuration[key] === null)
                result = 1;
            else if (this[key] !== null && configuration[key] !== null) {
                if (typeof this[key] === 'string')
                    result = this[key].localeCompare(configuration[key]);
                else if (typeof this[key] === 'number' || typeof this[key] === 'boolean')
                    result = this[key] - configuration[key];
                else
                    console.error(typeof this[key] + ' is not a recognized configuration type')
            }
        });
        return result;
    }
    compareSDKs(configuration) {
        if (this.sdk === null && configuration.sdk === null)
            return 0;
        if (this.sdk === null && configuration.sdk !== null)
            return -1;
        if (this.sdk !== null && configuration.sdk === null)
            return 1;

        const sdkRegex = /(\d+)([A-Z])(\d+)(.*)/;
        const myMatch = this.sdk.match(sdkRegex);
        const otherMatch = configuration.sdk.match(sdkRegex);

        if (!myMatch && !otherMatch) {
            console.error(`'${this.sdk}' and '${configuration.sdk}' do not match the SDK regular expression`);
            return this.sdk.localeCompare(configuration.sdk);
        }
        if (!myMatch && otherMatch)
            return -1;
        if (myMatch && !otherMatch)
            return 1;

        const majorDiff = parseInt(myMatch[1]) - parseInt(otherMatch[1]);
        if (majorDiff)
            return majorDiff;
        const minorDiff = myMatch[2].localeCompare(otherMatch[2]);
        if (minorDiff)
            return minorDiff;
        const buildDiff = parseInt(myMatch[3]) - parseInt(otherMatch[3]);
        if (buildDiff)
            return buildDiff;
        return myMatch[4].localeCompare(otherMatch[4]);
    }
    toParams() {
        return {
            platform: [this.platform],
            version:[this.version ? Configuration.integerToVersion(this.version) : null],
            version_name: [this.version_name],
            is_simulator: [this.is_simulator === null ? null : (this.is_simulator ? 'True' : 'False')],
            style: [this.style],
            flavor: [this.flavor],
            model: [this.model],
            architecture: [this.architecture],
        };
    }
}

export {Configuration};
