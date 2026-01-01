/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use byteorder::{BigEndian, WriteBytesExt};
use socket2::{Domain, Socket, Type};
use std::collections::HashMap;
use std::collections::LinkedList;
use std::ffi::{c_void, CStr, CString};
use std::io;
use std::net;
use std::os::raw::c_char;
use std::sync::mpsc::channel;
use std::thread;
use std::time;
use uuid::Uuid;

#[macro_use]
extern crate log;

struct Callback {
    data: *const c_void,
    resolved: unsafe extern "C" fn(*const c_void, *const c_char, *const c_char),
    timedout: unsafe extern "C" fn(*const c_void, *const c_char),
}

unsafe impl Send for Callback {}

fn hostname_resolved(callback: &Callback, hostname: &str, addr: &str) {
    if let Ok(hostname) = CString::new(hostname) {
        if let Ok(addr) = CString::new(addr) {
            unsafe {
                (callback.resolved)(callback.data, hostname.as_ptr(), addr.as_ptr());
            }
        }
    }
}

fn hostname_timedout(callback: &Callback, hostname: &str) {
    if let Ok(hostname) = CString::new(hostname) {
        unsafe {
            (callback.timedout)(callback.data, hostname.as_ptr());
        }
    }
}

// This code is derived from code for creating questions in the dns-parser
// crate. It would be nice to upstream this, or something similar.
fn create_answer(id: u16, answers: &[(String, &[u8])]) -> Result<Vec<u8>, io::Error> {
    let mut buf = Vec::with_capacity(512);
    let head = dns_parser::Header {
        id,
        query: false,
        opcode: dns_parser::Opcode::StandardQuery,
        authoritative: true,
        truncated: false,
        recursion_desired: false,
        recursion_available: false,
        authenticated_data: false,
        checking_disabled: false,
        response_code: dns_parser::ResponseCode::NoError,
        questions: 0,
        answers: answers.len() as u16,
        nameservers: 0,
        additional: 0,
    };

    buf.extend([0u8; 12].iter());
    head.write(&mut buf[..12]);

    for (name, addr) in answers {
        for part in name.split('.') {
            if part.len() > 62 {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Name part length too long",
                ));
            }
            let ln = part.len() as u8;
            buf.push(ln);
            buf.extend(part.as_bytes());
        }
        buf.push(0);

        if addr.len() == 4 {
            buf.write_u16::<BigEndian>(dns_parser::Type::A as u16)?;
        } else {
            buf.write_u16::<BigEndian>(dns_parser::Type::AAAA as u16)?;
        }
        // set cache flush bit
        buf.write_u16::<BigEndian>(dns_parser::Class::IN as u16 | (0x1 << 15))?;
        buf.write_u32::<BigEndian>(120)?;
        buf.write_u16::<BigEndian>(addr.len() as u16)?;
        buf.extend(*addr);
    }

    Ok(buf)
}

fn create_query(id: u16, queries: &[String]) -> Result<Vec<u8>, io::Error> {
    let mut buf = Vec::with_capacity(512);
    let head = dns_parser::Header {
        id,
        query: true,
        opcode: dns_parser::Opcode::StandardQuery,
        authoritative: false,
        truncated: false,
        recursion_desired: false,
        recursion_available: false,
        authenticated_data: false,
        checking_disabled: false,
        response_code: dns_parser::ResponseCode::NoError,
        questions: queries.len() as u16,
        answers: 0,
        nameservers: 0,
        additional: 0,
    };

    buf.extend([0u8; 12].iter());
    head.write(&mut buf[..12]);

    for name in queries {
        for part in name.split('.') {
            assert!(part.len() < 63);
            let ln = part.len() as u8;
            buf.push(ln);
            buf.extend(part.as_bytes());
        }
        buf.push(0);

        buf.write_u16::<BigEndian>(dns_parser::QueryType::A as u16)?;
        buf.write_u16::<BigEndian>(dns_parser::QueryClass::IN as u16)?;
    }

    Ok(buf)
}

fn handle_queries(
    socket: &std::net::UdpSocket,
    mdns_addr: &std::net::SocketAddr,
    pending_queries: &mut HashMap<String, Query>,
    unsent_queries: &mut LinkedList<Query>,
) {
    if pending_queries.len() < 50 {
        let mut queries: Vec<Query> = Vec::new();
        while queries.len() < 5 && !unsent_queries.is_empty() {
            if let Some(query) = unsent_queries.pop_front() {
                if !pending_queries.contains_key(&query.hostname) {
                    queries.push(query);
                }
            }
        }
        if !queries.is_empty() {
            let query_hostnames: Vec<String> =
                queries.iter().map(|q| q.hostname.to_string()).collect();

            if let Ok(buf) = create_query(0, &query_hostnames) {
                match socket.send_to(&buf, &mdns_addr) {
                    Ok(_) => {
                        for query in queries {
                            pending_queries.insert(query.hostname.to_string(), query);
                        }
                    }
                    Err(err) => {
                        warn!("Sending mDNS query failed: {}", err);
                        if err.kind() != io::ErrorKind::PermissionDenied {
                            for query in queries {
                                unsent_queries.push_back(query);
                            }
                        } else {
                            for query in queries {
                                hostname_timedout(&query.callback, &query.hostname);
                            }
                        }
                    }
                }
            }
        }
    }

    let now = time::Instant::now();
    let expired: Vec<String> = pending_queries
        .iter()
        .filter(|(_, query)| now.duration_since(query.timestamp).as_secs() >= 3)
        .map(|(hostname, _)| hostname.to_string())
        .collect();
    for hostname in expired {
        if let Some(mut query) = pending_queries.remove(&hostname) {
            query.attempts += 1;
            if query.attempts < 3 {
                query.timestamp = now;
                unsent_queries.push_back(query);
            } else {
                hostname_timedout(&query.callback, &hostname);
            }
        }
    }
}

fn handle_mdns_socket(
    socket: &std::net::UdpSocket,
    mdns_addr: &std::net::SocketAddr,
    mut buffer: &mut [u8],
    hosts: &mut HashMap<String, Vec<u8>>,
    pending_queries: &mut HashMap<String, Query>,
) -> bool {

    match socket.recv_from(&mut buffer) {
        Ok((amt, _)) => {
            if amt > 0 {
                let buffer = &buffer[0..amt];
                match dns_parser::Packet::parse(&buffer) {
                    Ok(parsed) => {
                        let mut answers: Vec<(String, &[u8])> = Vec::new();

                        // If a packet contains both both questions and
                        // answers, the questions should be ignored.
                        if parsed.answers.is_empty() {
                            parsed
                                .questions
                                .iter()
                                .filter(|question| question.qtype == dns_parser::QueryType::A)
                                .for_each(|question| {
                                    let qname = question.qname.to_string();
                                    trace!("mDNS question: {} {:?}", qname, question.qtype);
                                    if let Some(octets) = hosts.get(&qname) {
                                        trace!("Sending mDNS answer for {}: {:?}", qname, octets);
                                        answers.push((qname, &octets));
                                    }
                                });
                        }
                        for answer in parsed.answers {
                            let hostname = answer.name.to_string();
                            match pending_queries.get(&hostname) {
                                Some(query) => {
                                    match answer.data {
                                        dns_parser::RData::A(dns_parser::rdata::a::Record(
                                            addr,
                                        )) => {
                                            let addr = addr.to_string();
                                            trace!("mDNS response: {} {}", hostname, addr);
                                            hostname_resolved(&query.callback, &hostname, &addr);
                                        }
                                        dns_parser::RData::AAAA(
                                            dns_parser::rdata::aaaa::Record(addr),
                                        ) => {
                                            let addr = addr.to_string();
                                            trace!("mDNS response: {} {}", hostname, addr);
                                            hostname_resolved(&query.callback, &hostname, &addr);
                                        }
                                        _ => {}
                                    }
                                    pending_queries.remove(&hostname);
                                }
                                None => {
                                    continue;
                                }
                            }
                        }
                        // TODO: If we did not answer every query in this
                        // question, we should wait for a random amount of time
                        // so as to not collide with someone else responding to
                        // this query.
                        if !answers.is_empty() {
                            if let Ok(buf) = create_answer(parsed.header.id, &answers) {
                                if let Err(err) = socket.send_to(&buf, &mdns_addr) {
                                    warn!("Sending mDNS answer failed: {}", err);
                                }
                            }
                        }
                    }
                    Err(err) => {
                        warn!("Could not parse mDNS packet: {}", err);
                    }
                }
            }
        }
        Err(err) => {
            if err.kind() != io::ErrorKind::Interrupted
                && err.kind() != io::ErrorKind::TimedOut
                && err.kind() != io::ErrorKind::WouldBlock
            {
                error!("Socket error: {}", err);
                return false;
            }
        }
    }

    true
}

fn validate_hostname(hostname: &str) -> bool {
    match hostname.find(".local") {
        Some(index) => match hostname.get(0..index) {
            Some(uuid) => match uuid.get(0..36) {
                Some(initial) => match Uuid::parse_str(initial) {
                    Ok(_) => {
                        // Oddly enough, Safari does not generate valid UUIDs,
                        // the last part sometimes contains more than 12 digits.
                        match uuid.get(36..) {
                            Some(trailing) => {
                                for c in trailing.chars() {
                                    if !c.is_ascii_hexdigit() {
                                        return false;
                                    }
                                }
                                true
                            }
                            None => true,
                        }
                    }
                    Err(_) => false,
                },
                None => false,
            },
            None => false,
        },
        None => false,
    }
}

enum ServiceControl {
    Register {
        hostname: String,
        address: String,
    },
    Query {
        callback: Callback,
        hostname: String,
    },
    Unregister {
        hostname: String,
    },
    Stop,
}

struct Query {
    hostname: String,
    callback: Callback,
    timestamp: time::Instant,
    attempts: i32,
}

impl Query {
    fn new(hostname: &str, callback: Callback) -> Query {
        Query {
            hostname: hostname.to_string(),
            callback,
            timestamp: time::Instant::now(),
            attempts: 0,
        }
    }
}

pub struct MDNSService {
    handle: Option<std::thread::JoinHandle<()>>,
    sender: Option<std::sync::mpsc::Sender<ServiceControl>>,
}

impl MDNSService {
    fn register_hostname(&mut self, hostname: &str, address: &str) {
        if let Some(sender) = &self.sender {
            if let Err(err) = sender.send(ServiceControl::Register {
                hostname: hostname.to_string(),
                address: address.to_string(),
            }) {
                warn!(
                    "Could not send register hostname {} message: {}",
                    hostname, err
                );
            }
        }
    }

    fn query_hostname(&mut self, callback: Callback, hostname: &str) {
        if let Some(sender) = &self.sender {
            if let Err(err) = sender.send(ServiceControl::Query {
                callback,
                hostname: hostname.to_string(),
            }) {
                warn!(
                    "Could not send query hostname {} message: {}",
                    hostname, err
                );
            }
        }
    }

    fn unregister_hostname(&mut self, hostname: &str) {
        if let Some(sender) = &self.sender {
            if let Err(err) = sender.send(ServiceControl::Unregister {
                hostname: hostname.to_string(),
            }) {
                warn!(
                    "Could not send unregister hostname {} message: {}",
                    hostname, err
                );
            }
        }
    }

    fn start(&mut self, addrs: Vec<std::net::Ipv4Addr>) -> io::Result<()> {
        let (sender, receiver) = channel();
        self.sender = Some(sender);

        let mdns_addr = std::net::Ipv4Addr::new(224, 0, 0, 251);
        let port = 5353;

        let socket = Socket::new(Domain::IPV4, Type::DGRAM, None)?;
        socket.set_reuse_address(true)?;

        #[cfg(not(target_os = "windows"))]
        socket.set_reuse_port(true)?;
        socket.bind(&socket2::SockAddr::from(std::net::SocketAddr::from((
            [0, 0, 0, 0],
            port,
        ))))?;

        let socket = std::net::UdpSocket::from(socket);
        socket.set_multicast_loop_v4(true)?;
        socket.set_read_timeout(Some(time::Duration::from_millis(1)))?;
        socket.set_write_timeout(Some(time::Duration::from_millis(1)))?;
        for addr in addrs {
            if let Err(err) = socket.join_multicast_v4(&mdns_addr, &addr) {
                warn!(
                    "Could not join multicast group on interface: {:?}: {}",
                    addr, err
                );
            }
        }

        let thread_name = "mdns_service";
        let builder = thread::Builder::new().name(thread_name.into());
        self.handle = Some(builder.spawn(move || {
            let mdns_addr = std::net::SocketAddr::from(([224, 0, 0, 251], port));
            let mut buffer: [u8; 9_000] = [0; 9_000];
            let mut hosts = HashMap::new();
            let mut unsent_queries = LinkedList::new();
            let mut pending_queries = HashMap::new();
            loop {
                match receiver.try_recv() {
                    Ok(msg) => match msg {
                        ServiceControl::Register { hostname, address } => {
                            if !validate_hostname(&hostname) {
                                warn!("Not registering invalid hostname: {}", hostname);
                                continue;
                            }
                            trace!("Registering {} for: {}", hostname, address);
                            match address.parse().and_then(|ip| {
                                Ok(match ip {
                                    net::IpAddr::V4(ip) => ip.octets().to_vec(),
                                    net::IpAddr::V6(ip) => ip.octets().to_vec(),
                                })
                            }) {
                                Ok(octets) => {
                                    let mut v = Vec::new();
                                    v.extend(octets);
                                    hosts.insert(hostname, v);
                                }
                                Err(err) => {
                                    warn!(
                                        "Could not parse address for {}: {}: {}",
                                        hostname, address, err
                                    );
                                }
                            }
                        }
                        ServiceControl::Query { callback, hostname } => {
                            trace!("Querying {}", hostname);
                            if !validate_hostname(&hostname) {
                                warn!("Not sending mDNS query for invalid hostname: {}", hostname);
                                continue;
                            }
                            unsent_queries.push_back(Query::new(&hostname, callback));
                        }
                        ServiceControl::Unregister { hostname } => {
                            trace!("Unregistering {}", hostname);
                            hosts.remove(&hostname);
                        }
                        ServiceControl::Stop => {
                            trace!("Stopping");
                            break;
                        }
                    },
                    Err(std::sync::mpsc::TryRecvError::Disconnected) => {
                        break;
                    }
                    Err(std::sync::mpsc::TryRecvError::Empty) => {}
                }

                handle_queries(
                    &socket,
                    &mdns_addr,
                    &mut pending_queries,
                    &mut unsent_queries,
                );

                if !handle_mdns_socket(
                    &socket,
                    &mdns_addr,
                    &mut buffer,
                    &mut hosts,
                    &mut pending_queries,
                ) {
                    break;
                }
            }
        })?);

        Ok(())
    }

    fn stop(self) {
        if let Some(sender) = self.sender {
            if let Err(err) = sender.send(ServiceControl::Stop) {
                warn!("Could not stop mDNS Service: {}", err);
            }
            if let Some(handle) = self.handle {
                if handle.join().is_err() {
                    error!("Error on thread join");
                }
            }
        }
    }

    fn new() -> MDNSService {
        MDNSService {
            handle: None,
            sender: None,
        }
    }
}

/// # Safety
///
/// This function must only be called with a valid MDNSService pointer.
/// This hostname and address arguments must be zero terminated strings.
#[no_mangle]
pub unsafe extern "C" fn mdns_service_register_hostname(
    serv: *mut MDNSService,
    hostname: *const c_char,
    address: *const c_char,
) {
    assert!(!serv.is_null());
    assert!(!hostname.is_null());
    assert!(!address.is_null());
    let hostname = CStr::from_ptr(hostname).to_string_lossy();
    let address = CStr::from_ptr(address).to_string_lossy();
    (*serv).register_hostname(&hostname, &address);
}

/// # Safety
///
/// This ifaddrs argument must be a zero terminated string.
#[no_mangle]
pub unsafe extern "C" fn mdns_service_start(ifaddrs: *const c_char) -> *mut MDNSService {
    assert!(!ifaddrs.is_null());
    let mut r = Box::new(MDNSService::new());
    let ifaddrs = CStr::from_ptr(ifaddrs).to_string_lossy();
    let addrs: Vec<std::net::Ipv4Addr> =
        ifaddrs.split(';').filter_map(|x| x.parse().ok()).collect();

    if addrs.is_empty() {
        warn!("Could not parse interface addresses from: {}", ifaddrs);
    } else if let Err(err) = r.start(addrs) {
        warn!("Could not start mDNS Service: {}", err);
    }

    Box::into_raw(r)
}

/// # Safety
///
/// This function must only be called with a valid MDNSService pointer.
#[no_mangle]
pub unsafe extern "C" fn mdns_service_stop(serv: *mut MDNSService) {
    assert!(!serv.is_null());
    let boxed = Box::from_raw(serv);
    boxed.stop();
}

/// # Safety
///
/// This function must only be called with a valid MDNSService pointer.
/// The data argument will be passed back into the resolved and timedout
/// functions. The object it points to must not be freed until the MDNSService
/// has stopped.
#[no_mangle]
pub unsafe extern "C" fn mdns_service_query_hostname(
    serv: *mut MDNSService,
    data: *const c_void,
    resolved: unsafe extern "C" fn(*const c_void, *const c_char, *const c_char),
    timedout: unsafe extern "C" fn(*const c_void, *const c_char),
    hostname: *const c_char,
) {
    assert!(!serv.is_null());
    assert!(!data.is_null());
    assert!(!hostname.is_null());
    let hostname = CStr::from_ptr(hostname).to_string_lossy();
    let callback = Callback {
        data,
        resolved,
        timedout,
    };
    (*serv).query_hostname(callback, &hostname);
}

/// # Safety
///
/// This function must only be called with a valid MDNSService pointer.
/// This function should only be called once per hostname.
#[no_mangle]
pub unsafe extern "C" fn mdns_service_unregister_hostname(
    serv: *mut MDNSService,
    hostname: *const c_char,
) {
    assert!(!serv.is_null());
    assert!(!hostname.is_null());
    let hostname = CStr::from_ptr(hostname).to_string_lossy();
    (*serv).unregister_hostname(&hostname);
}

#[cfg(test)]
mod tests {
    use crate::create_query;
    use crate::validate_hostname;
    use crate::Callback;
    use crate::MDNSService;
    use socket2::{Domain, Socket, Type};
    use std::collections::HashSet;
    use std::ffi::c_void;
    use std::io;
    use std::iter::FromIterator;
    use std::os::raw::c_char;
    use std::thread;
    use std::time;
    use uuid::Uuid;

    #[no_mangle]
    pub unsafe extern "C" fn mdns_service_resolved(
        _: *const c_void,
        _: *const c_char,
        _: *const c_char,
    ) -> () {
    }

    #[no_mangle]
    pub unsafe extern "C" fn mdns_service_timedout(_: *const c_void, _: *const c_char) -> () {}

    fn listen_until(addr: &std::net::Ipv4Addr, stop: u64) -> thread::JoinHandle<Vec<String>> {
        let port = 5353;

        let socket = Socket::new(Domain::IPV4, Type::DGRAM, None).unwrap();
        socket.set_reuse_address(true).unwrap();

        #[cfg(not(target_os = "windows"))]
        socket.set_reuse_port(true).unwrap();
        socket
            .bind(&socket2::SockAddr::from(std::net::SocketAddr::from((
                [0, 0, 0, 0],
                port,
            ))))
            .unwrap();

        let socket = std::net::UdpSocket::from(socket);
        socket.set_multicast_loop_v4(true).unwrap();
        socket
            .set_read_timeout(Some(time::Duration::from_millis(10)))
            .unwrap();
        socket
            .set_write_timeout(Some(time::Duration::from_millis(10)))
            .unwrap();
        socket
            .join_multicast_v4(&std::net::Ipv4Addr::new(224, 0, 0, 251), &addr)
            .unwrap();

        let mut buffer: [u8; 9_000] = [0; 9_000];
        thread::spawn(move || {
            let start = time::Instant::now();
            let mut questions = Vec::new();
            while time::Instant::now().duration_since(start).as_secs() < stop {
                match socket.recv_from(&mut buffer) {
                    Ok((amt, _)) => {
                        if amt > 0 {
                            let buffer = &buffer[0..amt];
                            match dns_parser::Packet::parse(&buffer) {
                                Ok(parsed) => {
                                    parsed
                                        .questions
                                        .iter()
                                        .filter(|question| {
                                            question.qtype == dns_parser::QueryType::A
                                        })
                                        .for_each(|question| {
                                            let qname = question.qname.to_string();
                                            questions.push(qname);
                                        });
                                }
                                Err(err) => {
                                    warn!("Could not parse mDNS packet: {}", err);
                                }
                            }
                        }
                    }
                    Err(err) => {
                        if err.kind() != io::ErrorKind::WouldBlock
                            && err.kind() != io::ErrorKind::TimedOut
                        {
                            error!("Socket error: {}", err);
                            break;
                        }
                    }
                }
            }
            questions
        })
    }

    #[test]
    fn test_validate_hostname() {
        assert_eq!(
            validate_hostname("e17f08d4-689a-4df6-ba31-35bb9f041100.local"),
            true
        );
        assert_eq!(
            validate_hostname("62240723-ae6d-4f6a-99b8-94a233e3f84a2.local"),
            true
        );
        assert_eq!(
            validate_hostname("62240723-ae6d-4f6a-99b8.94e3f84a2.local"),
            false
        );
        assert_eq!(validate_hostname("hi there"), false);
    }

    #[test]
    fn start_stop() {
        let mut service = MDNSService::new();
        let addr = "127.0.0.1".parse().unwrap();
        service.start(vec![addr]).unwrap();
        service.stop();
    }

    #[test]
    fn simple_query() {
        let mut service = MDNSService::new();
        let addr = "127.0.0.1".parse().unwrap();
        let handle = listen_until(&addr, 1);

        service.start(vec![addr]).unwrap();

        let callback = Callback {
            data: 0 as *const c_void,
            resolved: mdns_service_resolved,
            timedout: mdns_service_timedout,
        };
        let hostname = Uuid::new_v4().as_hyphenated().to_string() + ".local";
        service.query_hostname(callback, &hostname);
        service.stop();
        let questions = handle.join().unwrap();
        assert!(questions.contains(&hostname));
    }

    #[test]
    fn rate_limited_query() {
        let mut service = MDNSService::new();
        let addr = "127.0.0.1".parse().unwrap();
        let handle = listen_until(&addr, 1);

        service.start(vec![addr]).unwrap();

        let mut hostnames = HashSet::new();
        for _ in 0..100 {
            let callback = Callback {
                data: 0 as *const c_void,
                resolved: mdns_service_resolved,
                timedout: mdns_service_timedout,
            };
            let hostname = Uuid::new_v4().as_hyphenated().to_string() + ".local";
            service.query_hostname(callback, &hostname);
            hostnames.insert(hostname);
        }
        service.stop();
        let questions = HashSet::from_iter(handle.join().unwrap().iter().map(|x| x.to_string()));
        let intersection: HashSet<&String> = questions.intersection(&hostnames).collect();
        assert_eq!(intersection.len(), 50);
    }

    #[test]
    fn repeat_failed_query() {
        let mut service = MDNSService::new();
        let addr = "127.0.0.1".parse().unwrap();
        let handle = listen_until(&addr, 4);

        service.start(vec![addr]).unwrap();

        let hostname = Uuid::new_v4().as_hyphenated().to_string() + ".local";
        let callback = Callback {
            data: 0 as *const c_void,
            resolved: mdns_service_resolved,
            timedout: mdns_service_timedout,
        };
        service.query_hostname(callback, &hostname);
        thread::sleep(time::Duration::from_secs(4));
        service.stop();

        let questions: Vec<String> = handle
            .join()
            .unwrap()
            .iter()
            .filter(|x| *x == &hostname)
            .map(|x| x.to_string())
            .collect();
        assert_eq!(questions.len(), 2);
    }

    #[test]
    fn multiple_queries_in_a_single_packet() {
        let mut hostnames: Vec<String> = Vec::new();
        for _ in 0..100 {
            let hostname = Uuid::new_v4().as_hyphenated().to_string() + ".local";
            hostnames.push(hostname);
        }

        match create_query(42, &hostnames) {
            Ok(q) => match dns_parser::Packet::parse(&q) {
                Ok(parsed) => {
                    assert_eq!(parsed.questions.len(), 100);
                }
                Err(_) => assert!(false),
            },
            Err(_) => assert!(false),
        }
    }
}
