global.CommonComponentBase = require('../../public/shared/common-component-base').CommonComponentBase;
global.MarkupPage = require('./markup-component.js').MarkupPage;
global.MarkupComponentBase = require('./markup-component.js').MarkupComponentBase;

const fs = require('fs');
const path = require('path');
const os = require('os');
const TestGroupResultPage = require('./test-group-result-page.js').TestGroupResultPage;

class AnalysisResultsNotifier {
    constructor(messageTemplate, finalizeScript, messageConstructionRules, notificationServerRemoteAPI, notificationServicePath, Subprocess)
    {
        this._messageTemplate = messageTemplate;
        this._notificationServerRemoteAPI = notificationServerRemoteAPI;
        this._notificationServicePath = notificationServicePath;
        this._subprocess = Subprocess;
        this._finalizeScript = finalizeScript;
        AnalysisResultsNotifier._validateRules(messageConstructionRules);
        this._messageConstructionRules = messageConstructionRules;
    }

    static _validateRules(rules)
    {
        function isNonemptyArrayOfStrings(object) {
            if (!object)
                return false;
            if (!Array.isArray(object))
                return false;
            return object.every((entry) => entry instanceof String || typeof(entry) === 'string');
        }
        for (const rule of rules)
            console.assert(isNonemptyArrayOfStrings(rule.platforms) || isNonemptyArrayOfStrings(rule.tests), 'Either tests or platforms should be an array of strings');
    }

    async sendNotificationsForTestGroups(testGroups)
    {
        for (const testGroup of testGroups) {
            await testGroup.fetchTask();
            const title = `"${testGroup.task().name()}" - "${testGroup.name()}" has finished`;
            const message = await AnalysisResultsNotifier._messageForTestGroup(testGroup, title);
            let content = AnalysisResultsNotifier._instantiateNotificationTemplate(this._messageTemplate, title, message);
            content = this._applyRules(testGroup.platform().name(), testGroup.test().path()[0].name(), !!testGroup.author(), content);
            const testGroupInfo = {author: testGroup.author()};

            const tempDir = fs.mkdtempSync(os.tmpdir());
            const tempFilePath = path.join(tempDir, 'temp-content.json');
            fs.writeFileSync(tempFilePath, JSON.stringify({content, testGroupInfo}));
            content = JSON.parse(await this._subprocess.execute([...this._finalizeScript, tempFilePath]));
            fs.unlinkSync(tempFilePath);
            fs.rmdirSync(tempDir);

            await this._sendNotification(content);
            await testGroup.didSendNotification();
        }
    }

    static async _messageForTestGroup(testGroup, title)
    {
        const page = new TestGroupResultPage(title);
        await page.setTestGroup(testGroup);

        return page.generateMarkup();
    }

    static _instantiateNotificationTemplate(template, title, message)
    {
        const instance = {};
        for (const name in template) {
            const value = template[name];
            if (typeof(value) === 'string')
                instance[name] = value.replace(/\$title/g, title).replace(/\$message/g, message);
            else if (typeof(template[name]) === 'object')
                instance[name] = this._instantiateNotificationTemplate(value, title, message);
            else
                instance[name] = value;
        }
        return instance;
    }

    _applyRules(platformName, testName, userInitiated, message)
    {
        for (const rule of this._messageConstructionRules) {
            if (AnalysisResultsNotifier._matchesRule(platformName, testName, userInitiated, rule))
                message = AnalysisResultsNotifier._applyUpdate(message, rule.parameters);
        }
        return message;
    }

    static _matchesRule(platform, test, userInitiated, rule)
    {
        if (rule.tests && !rule.tests.includes(test))
            return false;

        if (rule.platforms && !rule.platforms.includes(platform))
            return false;

        if ('userInitiated' in rule && userInitiated !== rule.userInitiated)
            return false;

        return true;
    }

    static _applyUpdate(message, update)
    {
        const messageType = typeof message;
        const updateType = typeof update;
        const supportedPrimitiveTypes = ["string", "number", "boolean"];
        const unsupportedPrimitiveTypes = ["symbol", "function", "undefined"];
        console.assert(!unsupportedPrimitiveTypes.includes(messageType) && !unsupportedPrimitiveTypes.includes(updateType));

        if (supportedPrimitiveTypes.includes(messageType) || supportedPrimitiveTypes.includes(updateType))
            return [message, update];

        for (let [key, value] of Object.entries(update)) {
            let mergedValue = null;
            let valueToMerge = message[key];

            if (!(key in message))
                mergedValue = value;
            else if (Array.isArray(value) || Array.isArray(valueToMerge)) {
                if (!Array.isArray(value))
                    value = [value];
                if (!Array.isArray(valueToMerge))
                    valueToMerge = [valueToMerge];
                mergedValue = [...valueToMerge, ...value];
            } else
                mergedValue = this._applyUpdate(valueToMerge, value);

            message[key] = mergedValue;
        }
        return message;
    }

    _sendNotification(content)
    {
        return this._notificationServerRemoteAPI.postJSON(this._notificationServicePath, content);
    }
}


if (typeof module !== 'undefined')
    module.exports.AnalysisResultsNotifier = AnalysisResultsNotifier;