/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

class LinkedList
{
    constructor()
    {
        this.head = new LinkedListNode;
        this.head.next = this.head.prev = this.head;
        this.length = 0;
    }

    clear()
    {
        this.head.next = this.head.prev = this.head;
        this.length = 0;
    }

    get last()
    {
        return this.head.prev;
    }

    push(item)
    {
        let newNode = new LinkedListNode(item);
        let last = this.last;
        let head = this.head;

        last.next = newNode;
        newNode.next = head;
        head.prev = newNode;
        newNode.prev = last;

        this.length++;

        return newNode;
    }

    remove(node)
    {
        if (!node)
            return false;

        node.prev.next = node.next;
        node.next.prev = node.prev;

        this.length--;
        return true;
    }

    forEach(callback)
    {
        let node = this.head;
        for (let i = 0, length = this.length; i < length; i++) {
            node = node.next;
            let returnValue = callback(node.value, i);
            if (returnValue === false)
                return;
        }
    }

    toArray()
    {
        let node = this.head;
        let i = this.length;
        let result = new Array(i);
        while (i--) {
            node = node.prev;
            result[i] = node.value;
        }
        return result;
    }

    toJSON()
    {
        return this.toArray();
    }
}


class LinkedListNode
{
    constructor(value)
    {
        this.value = value;
        this.prev = null;
        this.next = null;
    }
}
