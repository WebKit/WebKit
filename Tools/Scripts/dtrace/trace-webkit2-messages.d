#!/usr/sbin/dtrace -qZs

/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

struct WebKitMessageRecord {
    uint8_t sourceProcessType;
    pid_t sourceProcessID;

    uint8_t destinationProcessType;
    pid_t destinationProcessID;

    char* messageReceiverName;
    char* messageName;
    uint64_t destinationID;

    char UUID[16];

    double startTime;
    double endTime;

    bool isSyncMessage;
    bool shouldDispatchMessageWhenWaitingForSyncReply;
    bool isIncoming;
};

WebKitMessageRecorder*:::message_*
{
    this->record = (struct WebKitMessageRecord*)copyin(arg0, sizeof(struct WebKitMessageRecord));
    printf("{");

    printf("\"sourceProcessType\": %d, ", this->record->sourceProcessType);
    printf("\"sourceProcessID\": %d, ", this->record->sourceProcessID);

    printf("\"destinationProcessType\": %d, ", this->record->destinationProcessType);
    printf("\"destinationProcessID\": %d, ", this->record->destinationProcessID);

    printf("\"messageReceiverName\": \"%s\", ", copyinstr((user_addr_t)this->record->messageReceiverName));
    printf("\"messageName\": \"%s\", ", copyinstr((user_addr_t)this->record->messageName));

    printf("\"destinationID\": %d, ", this->record->destinationID);

    printf("\"UUID\": \"");
    printf("%02x", this->record->UUID[0]);
    printf("%02x", this->record->UUID[1]);
    printf("%02x", this->record->UUID[2]);
    printf("%02x", this->record->UUID[3]);
    printf("-");
    printf("%02x", this->record->UUID[4]);
    printf("%02x", this->record->UUID[5]);
    printf("-");
    printf("%02x", this->record->UUID[6]);
    printf("%02x", this->record->UUID[7]);
    printf("-");
    printf("%02x", this->record->UUID[8]);
    printf("%02x", this->record->UUID[9]);
    printf("-");
    printf("%02x", this->record->UUID[10]);
    printf("%02x", this->record->UUID[11]);
    printf("%02x", this->record->UUID[12]);
    printf("%02x", this->record->UUID[13]);
    printf("%02x", this->record->UUID[14]);
    printf("%02x", this->record->UUID[15]);
    printf("\", ");

    printf("\"startTime\": %f, ", this->record->startTime);
    printf("\"endTime\": %f, ", this->record->endTime);

    printf("\"isSyncMessage\": %d, ", this->record->isSyncMessage);
    printf("\"shouldDispatchMessageWhenWaitingForSyncReply\": %d, ", this->record->shouldDispatchMessageWhenWaitingForSyncReply);
    printf("\"isIncoming\": %d", this->record->isIncoming);

    printf("}\n");
}
