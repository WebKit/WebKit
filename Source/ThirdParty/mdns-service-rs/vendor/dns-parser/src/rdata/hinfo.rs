#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct Record;

impl<'a> super::Record<'a> for Record {

    const TYPE: isize = 13;

    fn parse(_rdata: &'a [u8], _original: &'a [u8]) -> super::RDataResult<'a> {
        unimplemented!();
    }
}
