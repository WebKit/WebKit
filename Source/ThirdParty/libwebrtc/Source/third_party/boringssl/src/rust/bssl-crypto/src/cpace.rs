// Copyright 2026 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! CPace
//!
//! This is an implementation of [CPace IETF draft].
//!
//! CPace is a quantum-annoying balanced Password-Authenticated Key Exchange scheme.
//!
//! ```
//! # use bssl_crypto::cpace::{Context, Role};
//!
//! let password = b"Password";
//! let channel_id = b"ChannelId";
//! let session_id = b"SessionId";
//! let alice_ad = b"ADa";
//! let bob_ad = b"ADb";
//!
//! // Alice (initiator) starts the exchange and generates a message.
//! let (alice_msg, alice_ctx) = Context::new(
//!     Role::Initiator,
//!     password,
//!     channel_id,
//!     alice_ad,
//!     session_id,
//! );
//!
//! // Bob (responder) starts the exchange and generates a message.
//! let (bob_msg, bob_ctx) = Context::new(
//!     Role::Responder,
//!     password,
//!     channel_id,
//!     bob_ad,
//!     session_id,
//! );
//!
//! // Alice and Bob exchange messages and associated data to derive the shared secret.
//! let alice_key = alice_ctx.finalize(&bob_msg, bob_ad).unwrap();
//! let bob_key = bob_ctx.finalize(&alice_msg, alice_ad).unwrap();
//! assert_eq!(alice_key, bob_key);
//! ```
//!
//! [CPace IETF draft]: https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-cpace

use core::{
    mem::MaybeUninit,
    ptr::{
        null_mut,
        NonNull, //
    }, //
};

use crate::FfiSlice;

/// CPace context for a single protocol execution.
pub struct Context(NonNull<bssl_sys::CPACE_CTX>);

// Safety: this context data does not use thread-local storage
unsafe impl Send for Context {}

// Safety: public API guarantees that the access will be thread-safe
unsafe impl Sync for Context {}

/// The role that a party plays in the CPace key agreement protocol.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    /// Initiator role, corresponding to party A in the specification.
    Initiator,
    /// Responder role, corresponding to party B in the specification.
    Responder,
}

impl From<Role> for bssl_sys::cpace_role_t {
    fn from(role: Role) -> Self {
        match role {
            Role::Initiator => bssl_sys::cpace_role_t_cpace_role_initiator,
            Role::Responder => bssl_sys::cpace_role_t_cpace_role_responder,
        }
    }
}

/// A CPace message to be sent to the peer.
pub type Message = [u8; bssl_sys::CPACE_MSG_LEN as usize];

/// The shared secret, ISK, derived from a CPace exchange.
pub type Output = [u8; bssl_sys::CPACE_SHARED_SECRET_LEN as usize];

impl Drop for Context {
    fn drop(&mut self) {
        unsafe {
            // Safety: `self.0` is still a valid point at this point.
            bssl_sys::CPACE_CTX_free(self.0.as_ptr());
        }
    }
}

impl Context {
    /// Create a new CPace context and generate the message to send to the peer.
    pub fn new(
        role: Role,
        password: &[u8],
        channel_id: &[u8],
        assoc_data: &[u8],
        session_id: &[u8],
    ) -> (Message, Self) {
        let (password, password_len) = (password.as_ffi_ptr(), password.len());
        let (channel_id, channel_id_len) = (channel_id.as_ffi_ptr(), channel_id.len());
        let (assoc_data, assoc_data_len) = (assoc_data.as_ffi_ptr(), assoc_data.len());
        let (sess_id, sess_id_len) = (session_id.as_ffi_ptr(), session_id.len());
        let ptr = unsafe {
            // Safety: all pointer and length pairs reference valid slices for the
            // duration of the call, and `CPACE_CTX_new` copies the inputs.
            bssl_sys::CPACE_CTX_new(
                role.into(),
                password,
                password_len,
                channel_id,
                channel_id_len,
                assoc_data,
                assoc_data_len,
                sess_id,
                sess_id_len,
            )
        };
        let ptr = NonNull::new(ptr).expect("allocation failed");
        let mut msg = [MaybeUninit::uninit(); bssl_sys::CPACE_MSG_LEN as usize];
        let rc = unsafe {
            // Safety: `ptr` is a valid newly initialized `CPACE_CTX` and `msg` has
            // `CPACE_MSG_LEN` bytes of space.
            bssl_sys::CPACE_generate_msg(ptr.as_ptr(), msg.as_mut_ptr() as *mut _)
        };
        assert!(rc == 1, "unexpected failure of message generation");
        (
            unsafe {
                // Safety: By BoringSSL invariant, we always write out some point data to the out
                // pointer.
                MaybeUninit::from(msg).assume_init()
            },
            Self(ptr),
        )
    }

    /// Finalize the CPace protocol by deriving the shared secret.
    ///
    /// This function returns error if `peer_message` is an invalid message.
    pub fn finalize(self, peer_message: &Message, peer_assoc_data: &[u8]) -> Result<Output, ()> {
        let (peer_assoc_data, peer_assoc_data_len) =
            (peer_assoc_data.as_ffi_ptr(), peer_assoc_data.len());
        let mut out = [MaybeUninit::uninit(); bssl_sys::CPACE_SHARED_SECRET_LEN as usize];
        let rc = unsafe {
            // Safety:
            // - `self` witnesses the validity of `self.0`
            // - `peer_message` and `peer_assoc_data` do not escape the call
            // - type state transition ensures that we are at the correct state of the protocol.
            bssl_sys::CPACE_process_msg(
                self.0.as_ptr(),
                peer_message.as_ptr(),
                peer_assoc_data,
                peer_assoc_data_len,
                out.as_mut_ptr() as *mut _,
                null_mut(),
            )
        };
        if rc == 1 {
            unsafe {
                // Safety: By BoringSSL invariant, we always write out some point data to the out
                // pointer if the operation was successful.
                Ok(MaybeUninit::from(out).assume_init())
            }
        } else {
            unsafe {
                // Safety: we are only clearing a thread-local error queue.
                bssl_sys::ERR_clear_error();
            }
            Err(())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const PASSWORD: &[u8] = b"Password";
    const CHANNEL_ID: &[u8] = b"some channel";
    const ADA: &[u8] = b"ADa";
    const ADB: &[u8] = b"ADb";
    const SID: &[u8] = &[
        0x7e, 0x4b, 0x47, 0x91, 0xd6, 0xa8, 0xef, 0x01, 0x9b, 0x93, 0x6c, 0x79, 0xfb, 0x7f, 0x2c,
        0x57,
    ];

    #[test]
    fn key_exchange() -> Result<(), ()> {
        let (alice_msg, alice) = Context::new(Role::Initiator, PASSWORD, CHANNEL_ID, ADA, SID);
        let (bob_msg, bob) = Context::new(Role::Responder, PASSWORD, CHANNEL_ID, ADB, SID);
        let alice_key = alice.finalize(&bob_msg, ADB)?;
        let bob_key = bob.finalize(&alice_msg, ADA)?;
        assert_eq!(alice_key.as_ref(), bob_key.as_ref());
        Ok(())
    }
}
