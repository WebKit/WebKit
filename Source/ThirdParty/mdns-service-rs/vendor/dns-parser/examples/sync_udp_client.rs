extern crate dns_parser;

use std::env;
use std::error::Error;
use std::net::UdpSocket;
use std::process;


use dns_parser::{Builder, Packet, RData, ResponseCode};
use dns_parser::rdata::a::Record;
use dns_parser::{QueryType, QueryClass};


fn main() {
    let mut code = 0;
    for name in env::args().skip(1) {
        match resolve(&name) {
            Ok(()) => {},
            Err(e) => {
                eprintln!("Error resolving {:?}: {}", name, e);
                code = 1;
            }
        }
    }
    process::exit(code);
}

fn resolve(name: &str) -> Result<(), Box<Error>> {
    let sock = UdpSocket::bind("127.0.0.1:0")?;
    sock.connect("127.0.0.1:53")?;
    let mut builder = Builder::new_query(1, true);
    builder.add_question(name, false, QueryType::A, QueryClass::IN);
    let packet = builder.build().map_err(|_| "truncated packet")?;
    sock.send(&packet)?;
    let mut buf = vec![0u8; 4096];
    sock.recv(&mut buf)?;
    let pkt = Packet::parse(&buf)?;
    if pkt.header.response_code != ResponseCode::NoError {
        return Err(pkt.header.response_code.into());
    }
    if pkt.answers.len() == 0 {
        return Err("No records received".into());
    }
    for ans in pkt.answers {
        match ans.data {
            RData::A(Record(ip)) => {
                println!("{}", ip);
            }
            _ => {} // ignore
        }
    }
    Ok(())
}
