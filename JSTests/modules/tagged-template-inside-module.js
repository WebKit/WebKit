import { shouldThrow, shouldBe } from "./resources/assert.js";
import { otherTaggedTemplates } from "./tagged-template-inside-module/other-tagged-templates.js"

function call(site) {
    return site;
}

var template = otherTaggedTemplates();
shouldBe(call`Cocoa` !== template, true);
shouldBe(template, otherTaggedTemplates());
shouldBe(template, new otherTaggedTemplates());
