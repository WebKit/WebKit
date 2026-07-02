/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
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

import { autorun } from "mobx";
import { Person } from "./Person.mjs";
import { Room } from "./Room.mjs";
import { Message } from "./Message.mjs";
import { Notifications } from "./Notifications.mjs";

function assert(condition) {
  if (!condition) throw new Error("Assertion failure");
}

export function runTest() {
  const logs = [];
  const customLog = (...args) => logs.push(args.join(" "));

  // --- Setup ---
  const techRoom = new Room("Tech Talk");
  const generalRoom = new Room("General Chat");
  const largeRoom = new Room("Large Meeting Room");

  const alice = new Person(1, "Alice");
  const bob = new Person(2, "Bob");

  techRoom.addPerson(alice);
  techRoom.addPerson(bob);
  generalRoom.addPerson(alice);

  const rooms = [techRoom, generalRoom, largeRoom];
  const notifications = new Notifications(rooms);

  // --- Large Room Setup ---
  customLog("\n--- Setting up Large Meeting Room ---");
  const largeRoomMembers = [];
  for (let i = 3; i < 203; i++) {
    const person = new Person(i, `Person ${i}`);
    largeRoomMembers.push(person);
    largeRoom.addPerson(person);
  }
  customLog(`[INFO] Added ${largeRoom.peopleCount} members to the large room.`);

  // Add Alice and Bob to the large room
  largeRoom.addPerson(alice);
  largeRoom.addPerson(bob);
  customLog("[INFO] Added Alice and Bob to the Large Meeting Room.");

  // Verify Person.rooms with Set
  assert(alice.rooms.size === 3);
  assert(
    alice.rooms.has(techRoom) &&
      alice.rooms.has(generalRoom) &&
      alice.rooms.has(largeRoom)
  );
  assert(
    bob.rooms.size === 2 && bob.rooms.has(techRoom) && bob.rooms.has(largeRoom)
  );
  customLog("[PASS] Verified Person.rooms associations with Set.");

  // --- autorun for global notifications ---
  const notificationsDisposer = autorun(() => {
    customLog("\n--- Global Notifications (Last Messages) ---");
    if (notifications.lastMessages.size === 0) {
      customLog("No messages yet.");
    } else {
      notifications.lastMessages.forEach((message, roomName) => {
        customLog(`[${roomName}] "${message.text}" - ${message.author.name}`);
      });
    }
    customLog("------------------------------------------");
  });

  // --- autorun for room updates ---
  const roomUpdatesDisposer = autorun(() => {
    rooms.forEach((room) => {
      if (room.roomUpdates.length > 0) {
        customLog(
          `[${room.name} Update] ${
            room.roomUpdates[room.roomUpdates.length - 1]
          }`
        );
      }
    });
  });

  // --- Status Update Scenario ---
  customLog("\n--- Status Update Scenario ---");

  alice.setStatus("Away");
  assert(techRoom.roomUpdates.includes("Alice is now Away"));
  assert(generalRoom.roomUpdates.includes("Alice is now Away"));
  assert(largeRoom.roomUpdates.includes("Alice is now Away"));
  customLog(
    "[PASS] Verified that status updates are sent to all of Alice's rooms."
  );

  bob.setStatus("Busy");
  assert(techRoom.roomUpdates.includes("Bob is now Busy"));
  assert(!generalRoom.roomUpdates.includes("Bob is now Busy"));
  assert(largeRoom.roomUpdates.includes("Bob is now Busy"));
  customLog(
    "[PASS] Verified that status updates are only sent to Bob's rooms."
  );

  // --- Messaging and Verification ---
  customLog("\n--- Sending Messages & Verifying Notifications ---");

  // Message 1: Tech Room
  const msg1 = new Message("First message to Tech Talk!", alice);
  techRoom.addMessage(msg1);
  assert(notifications.lastMessages.get("Tech Talk") === msg1);
  customLog("[PASS] Verified Tech Talk notification.");

  // Message 2: General Chat
  for (let i = 0; i < 20; i++) {
    const msg2 = new Message(`ping ${i}`, alice);
    generalRoom.addMessage(msg2);
    assert(notifications.lastMessages.get("General Chat") === msg2);
  }
  customLog("[PASS] Verified General Chat notification.");

  // Message 3: Large Meeting Room
  for (let i = 0; i <= 5; i++) {
    for (const member of largeRoomMembers) {
      const msg = new Message(`Hi, I am ${member.name}`, member);
      largeRoom.addMessage(msg);
      assert(largeRoom.lastMessage == msg);
      assert(notifications.lastMessages.get("Large Meeting Room") === msg);
    }
  }
  customLog("[PASS] Verified Large Meeting Room notification.");

  // Message 4: Tech Room again
  for (let i = 0; i < 20; i++) {
    const msg4 = new Message(`ping ${i}`, bob);
    techRoom.addMessage(msg4);
    assert(notifications.lastMessages.get("Tech Talk") === msg4);
  }
  customLog("[PASS] Verified Tech Talk notification update.");

  const tempRooms = [];
  for (let i = 0; i < 10; i++) {
    const roomId = `General Chat ${i}`;
    const tempRoom = new Room(roomId);
    tempRooms.push(tempRoom);
    notifications.addRoom(tempRoom);
    for (const person of largeRoomMembers.slice(0, 10)) {
      tempRoom.addPerson(person);
    }
    for (const person of tempRoom.members.slice(0, 5)) {
      const msg = new Message(`ping ${i}`, person);
      tempRoom.addMessage(msg);
      assert(notifications.lastMessages.get(roomId) === msg);
    }
  }
  tempRooms.forEach((room) => room.dispose(notifications));

  // --- Clean up ---
  notificationsDisposer();
  roomUpdatesDisposer(); // Added disposer for room updates
  techRoom.dispose(notifications);
  generalRoom.dispose(notifications);
  largeRoom.dispose(notifications);

  return logs;
}
