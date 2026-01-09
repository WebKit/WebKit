use std::net::Ipv4Addr;

use Error;
use byteorder::{BigEndian, ByteOrder};

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct Record(pub Ipv4Addr);

impl<'a> super::Record<'a> for Record {

    const TYPE: isize = 1;

    fn parse(rdata: &'a [u8], _original: &'a [u8]) -> super::RDataResult<'a> {
        if rdata.len() != 4 {
            return Err(Error::WrongRdataLength);
        }
        let address = Ipv4Addr::from(BigEndian::read_u32(rdata));
        let record = Record(address);
        Ok(super::RData::A(record))
    }
}
