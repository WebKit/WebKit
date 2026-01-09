extern crate dns_parser;

use std::env;
use std::error::Error;
use std::io::{Read, Write};
use std::net::TcpStream;
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
    let mut conn = TcpStream::connect("127.0.0.1:53")?;
    let mut builder = Builder::new_query(1, true);
    builder.add_question(name, false, QueryType::A, QueryClass::IN);
    let packet = builder.build().map_err(|_| "truncated packet")?;
    let psize = [((packet.len() >> 8) & 0xFF) as u8,
                 (packet.len() & 0xFF) as u8];
    conn.write_all(&psize[..])?;
    conn.write_all(&packet)?;
    let mut buf = vec![0u8; 4096];
    let mut off = 0;
    let pkt = loop {
        if buf.len() - off < 4096 {
            buf.extend(&[0u8; 4096][..]);
        }
        match conn.read(&mut buf[off..]) {
            Ok(num) => {
                off += num;
                if off < 2 { continue; }
                let bytes = ((buf[0] as usize) << 8) | buf[1] as usize;
                if off < bytes + 2 {
                    continue;
                }
                if num == 0 {
                    return Err("Partial packet received".into());
                }
                break Packet::parse(&buf[2..off])?;
            }
            Err(e) => {
                return Err(Box::new(e));
            }
        }
    };
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
