// Copyright (C) 2025 Apple Inc. All rights reserved.
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

const COMMIT_URL = 'https://commits.webkit.org/';
const VALID_REF_RE = /^[a-zA-Z0-9\.\@-][a-zA-Z0-9\/\.\@-]*$/;

function CreateElement(tag, {className, href, target, text} = {}, children = []) {
    const element = document.createElement(tag);
    if (className)
        element.className = className;
    if (href)
        element.href = href;
    if (target)
        element.target = target;
    if (text)
        element.textContent = text;
    element.append(...children);
    return element;
}

function ErrorMessage(...lines) {
    const children = [];
    for (const line of lines) {
        children.push(line, document.createElement('br'));
    }
    return CreateElement('div', {className: 'text small failed'}, children);
}

function EncodeRef(ref) {
    return ref.split('/').map(encodeURIComponent).join('/');
}

async function GetCommit(commit) {
    const response = await fetch(`/${EncodeRef(commit)}/json`);
    if (!response.ok) {
        throw new RangeError(`Could not find ${commit}.`);
    } else {
        const commitObj = await response.json();
        return commitObj;
    }
}

function ToggleButtons() {
    let hasInput = true;
    for (const commit of document.getElementsByName(`${this.name}`).values()) {
        if (!commit.value) {
            hasInput = false;
        }
    }
    for (const button of document.getElementsByName(`${this.name}Button`).values()) {
        if (hasInput) {
            button.disabled = false;
        } else {
            button.disabled = true;
        }
    }
}

function ValidateCommitFormat() {
    const commitRep = this.value;
    const message = document.getElementById(`${this.name}Message`)

    if (VALID_REF_RE.test(commitRep) || !commitRep) {
        message.replaceChildren();
        document.getElementById(this.id).classList.remove('invalid');
    } else {
        message.replaceChildren(CreateElement('div', {className: 'text small failed', text: `${commitRep} is not a valid commit format`}));
        document.getElementById(this.id).classList.add('invalid');
    }
}

async function GenerateCompareLink(validateCommits=false) {
    const commitOne = document.getElementById('commit1').value;
    const commitTwo = document.getElementById('commit2').value;
    const message = document.getElementById('compareMessage');

    if (!VALID_REF_RE.test(commitOne)) {
        message.replaceChildren(ErrorMessage(`${commitOne} is not a valid commit format`));
    } else if (!VALID_REF_RE.test(commitTwo)) {
        message.replaceChildren(ErrorMessage(`${commitTwo} is not a valid commit format`));
    } else if (validateCommits == true) {
        try {
            const commitOneData = await GetCommit(commitOne);
            const commitTwoData = await GetCommit(commitTwo);
            return COMMIT_URL + 'compare/' + commitOneData.identifier + '...' + commitTwoData.identifier;
        } catch (error) {
            console.error(error);
            if (error instanceof RangeError)
                message.replaceChildren(ErrorMessage(error.message, 'Please fix any typos or check if this commit exists.'));
            else
                message.replaceChildren(ErrorMessage(error.message));
        }
    } else {
        return COMMIT_URL + 'compare/' + commitOne + '...' + commitTwo;
    }
    return;
}

async function CopyCompareLink() {
    // Creating a Promise as a workaround for https://bugs.webkit.org/show_bug.cgi?id=222262
    navigator.clipboard.write([
        new ClipboardItem({
            "text/plain": new Promise(async resolve => {
                const compareLink = await GenerateCompareLink();
                if (compareLink) {
                    const message = document.getElementById('compareMessage');
                    message.textContent = 'Copied to clipboard!';
                    resolve(compareLink);
                }
            })
        })
    ]);

    const formattedLink = await GenerateCompareLink(true);
    if (formattedLink) {
        const message = document.getElementById('compareMessage');
        message.append(document.createElement('br'), CreateElement('a', {href: formattedLink, text: ` ${formattedLink} `}));
    }
}

async function CompareRedirect() {
    let compareLink = await GenerateCompareLink(true);
    if (compareLink) {
        window.location.href = compareLink;
    }
}

function FindCommit() {
    const commit = document.getElementById('commitSearch').value;
    const message = document.getElementById('searchMessage');
    if (!commit) {
        message.replaceChildren(ErrorMessage('Must provide a commit identifier'));
        return;
    }
    if (!VALID_REF_RE.test(commit)) {
        message.replaceChildren(ErrorMessage(`${commit} is not a valid commit format`));
        return;
    }

    fetch(`/${EncodeRef(commit)}/json`).then(response => {
        if (!response.ok) {
            message.replaceChildren(ErrorMessage(`Could not find ${commit}.`, 'Please fix any typos or check if this commit exists.'));
        } else {
            response.json().then(data => {
                const content = CreateElement('div', {className: 'content'}, [
                    CreateElement('a', {href: COMMIT_URL + EncodeRef(commit), target: '_blank', text: ` ${data.identifier} `}),
                    ` (${data.hash})`,
                ]);
                if (data.author) {
                    const email = data.author.emails?.[0];
                    const author = CreateElement('p', {}, [` Author: ${data.author.name} `]);
                    if (email)
                        author.append('<', CreateElement('a', {href: `mailto:${email}`, text: email}), '> ');
                    content.append(author);
                }
                if (data.message) {
                    content.append(CreateElement('p', {text: ` ${data.message.split('\n')[0]}`}));
                }
                message.replaceChildren(CreateElement('div', {className: 'col-8'}, [
                    CreateElement('div', {className: 'section article'}, [content]),
                ]));
            }).catch(error => {
                console.error(error);
            });
        }
    }).catch(error => {
        console.error(error);
    });
}

export {FindCommit, CompareRedirect, CopyCompareLink, ValidateCommitFormat, ToggleButtons};
