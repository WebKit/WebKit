use Name;

#[derive(Debug, Clone, Copy)]
pub struct Record<'a>(pub Name<'a>);

impl<'a> ToString for Record<'a> {
    #[inline]
    fn to_string(&self) -> String {
        self.0.to_string()
    }
}

impl<'a> super::Record<'a> for Record<'a> {

    const TYPE: isize = 2;

    fn parse(rdata: &'a [u8], original: &'a [u8]) -> super::RDataResult<'a> {
        let name = Name::scan(rdata, original)?;
        let record = Record(name);
        Ok(super::RData::NS(record))
    }
}

#[cfg(test)]
mod test {

    use {Packet, Header};
    use Opcode::*;
    use ResponseCode::NoError;
    use QueryType as QT;
    use QueryClass as QC;
    use Class as C;
    use RData;

    #[test]
    fn parse_response() {
        let response = b"\x4a\xf0\x81\x80\x00\x01\x00\x01\x00\x01\x00\x00\
                         \x03www\x05skype\x03com\x00\x00\x01\x00\x01\
                         \xc0\x0c\x00\x05\x00\x01\x00\x00\x0e\x10\
                         \x00\x1c\x07\x6c\x69\x76\x65\x63\x6d\x73\x0e\x74\
                         \x72\x61\x66\x66\x69\x63\x6d\x61\x6e\x61\x67\x65\
                         \x72\x03\x6e\x65\x74\x00\
                         \xc0\x42\x00\x02\x00\x01\x00\x01\xd5\xd3\x00\x11\
                         \x01\x67\x0c\x67\x74\x6c\x64\x2d\x73\x65\x72\x76\x65\x72\x73\
                         \xc0\x42";
         let packet = Packet::parse(response).unwrap();
         assert_eq!(packet.header, Header {
             id: 19184,
             query: false,
             opcode: StandardQuery,
             authoritative: false,
             truncated: false,
             recursion_desired: true,
             recursion_available: true,
             authenticated_data: false,
             checking_disabled: false,
             response_code: NoError,
             questions: 1,
             answers: 1,
             nameservers: 1,
             additional: 0,
         });
         assert_eq!(packet.questions.len(), 1);
         assert_eq!(packet.questions[0].qtype, QT::A);
         assert_eq!(packet.questions[0].qclass, QC::IN);
         assert_eq!(&packet.questions[0].qname.to_string()[..], "www.skype.com");
         assert_eq!(packet.answers.len(), 1);
         assert_eq!(&packet.answers[0].name.to_string()[..], "www.skype.com");
         assert_eq!(packet.answers[0].cls, C::IN);
         assert_eq!(packet.answers[0].ttl, 3600);
         match packet.answers[0].data {
             RData::CNAME(cname) => {
                 assert_eq!(&cname.0.to_string()[..], "livecms.trafficmanager.net");
             }
             ref x => panic!("Wrong rdata {:?}", x),
         }
         assert_eq!(packet.nameservers.len(), 1);
         assert_eq!(&packet.nameservers[0].name.to_string()[..], "net");
         assert_eq!(packet.nameservers[0].cls, C::IN);
         assert_eq!(packet.nameservers[0].ttl, 120275);
         match packet.nameservers[0].data {
             RData::NS(ns) => {
                 assert_eq!(&ns.0.to_string()[..], "g.gtld-servers.net");
             }
             ref x => panic!("Wrong rdata {:?}", x),
         }
     }
}
