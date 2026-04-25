/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

export default class AsyncTaskQueue {
    constructor(limit = Infinity)
    {
        this._tasks = [];
        this._limit = limit;
        this._setUpCondition();
    }

    async take()
    {
        while (!this._tasks.length)
            await this._conditionPromise;
        let task = this._tasks.shift();
        this._wakeUp();
        return task;
    }

    async post(task)
    {
        while (this._tasks.length >= this._limit)
            await this._conditionPromise;
        return this._postTask(task);
    }

    async postOrFailWhenExceedingLimit(task)
    {
        if (this._tasks.length >= this._limit)
            throw new Error("Too many tasks are queued");
        return this._postTask(task);
    }

    get length()
    {
        return this._tasks.length;
    }

    _postTask(task)
    {
        return new Promise((resolve, reject) => {
            this._tasks.push({
                task,
                resolve,
                reject,
            });
            this._wakeUp();
        });
    }

    _setUpCondition()
    {
        this._conditionPromise = new Promise((resolve) => {
            this._conditionPromiseResolve = resolve;
        });
    }

    _wakeUp()
    {
        let resolve = this._conditionPromiseResolve;
        this._setUpCondition();
        resolve();
    }
}
