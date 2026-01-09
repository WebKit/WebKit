use std::fmt;
use std::fmt::Write;
use std::str::from_utf8;

// Deprecated since rustc 1.23
#[allow(unused_imports, deprecated)]
use std::ascii::AsciiExt;

use byteorder::{BigEndian, ByteOrder};

use {Error};

/// The DNS name as stored in the original packet
///
/// This contains just a reference to a slice that contains the data.
/// You may turn this into a string using `.to_string()`
#[derive(Clone, Copy)]
pub struct Name<'a>{
    labels: &'a [u8],
    /// This is the original buffer size. The compressed names in original
    /// are calculated in this buffer
    original: &'a [u8],
}

impl<'a> Name<'a> {
    /// Scan the data to get Name object
    ///
    /// The `data` should be a part of `original` where name should start.
    /// The `original` is the data starting a the start of a packet, so
    /// that offsets in compressed name starts from the `original`.
    pub fn scan(data: &'a[u8], original: &'a[u8]) -> Result<Name<'a>, Error> {
        let mut parse_data = data;
        let mut return_pos = None;
        let mut pos = 0;
        if parse_data.len() <= pos {
            return Err(Error::UnexpectedEOF);
        }
        // By setting the largest_pos to be the original len, a side effect
        // is that the pos variable can move forwards in the buffer once.
        let mut largest_pos = original.len();
        let mut byte = parse_data[pos];
        while byte != 0 {
            if parse_data.len() <= pos {
                return Err(Error::UnexpectedEOF);
            }
            if byte & 0b1100_0000 == 0b1100_0000 {
                if parse_data.len() < pos+2 {
                    return Err(Error::UnexpectedEOF);
                }
                let off = (BigEndian::read_u16(&parse_data[pos..pos+2])
                           & !0b1100_0000_0000_0000) as usize;
                if off >= original.len() {
                    return Err(Error::UnexpectedEOF);
                }
                // Set value for return_pos which is the pos in the original
                // data buffer that should be used to return after validating
                // the offsetted labels.
                if let None = return_pos {
                    return_pos = Some(pos);
                }

                // Check then set largest_pos to ensure we never go backwards
                // in the buffer.
                if off >= largest_pos {
                    return Err(Error::BadPointer);
                }
                largest_pos = off;
                pos = 0;
                parse_data = &original[off..];
            } else if byte & 0b1100_0000 == 0 {
                let end = pos + byte as usize + 1;
                if parse_data.len() < end {
                    return Err(Error::UnexpectedEOF);
                }
                if !parse_data[pos+1..end].is_ascii() {
                    return Err(Error::LabelIsNotAscii);
                }
                pos = end;
                if parse_data.len() <= pos {
                    return Err(Error::UnexpectedEOF);
                }
            } else {
                return Err(Error::UnknownLabelFormat);
            }
            byte = parse_data[pos];
        }
        if let Some(return_pos) = return_pos {
            return Ok(Name {labels: &data[..return_pos+2], original: original});
        } else {
            return Ok(Name {labels: &data[..pos+1], original: original });
        }
    }
    /// Number of bytes serialized name occupies
    pub fn byte_len(&self) -> usize {
        self.labels.len()
    }
}

impl<'a> fmt::Display for Name<'a> {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let data = self.labels;
        let original = self.original;
        let mut pos = 0;
        loop {
            let byte = data[pos];
            if byte == 0 {
                return Ok(());
            } else if byte & 0b1100_0000 == 0b1100_0000 {
                let off = (BigEndian::read_u16(&data[pos..pos+2])
                           & !0b1100_0000_0000_0000) as usize;
                if pos != 0 {
                    try!(fmt.write_char('.'));
                }
                return fmt::Display::fmt(
                    &Name::scan(&original[off..], original).unwrap(), fmt)
            } else if byte & 0b1100_0000 == 0 {
                if pos != 0 {
                    try!(fmt.write_char('.'));
                }
                let end = pos + byte as usize + 1;
                try!(fmt.write_str(from_utf8(&data[pos+1..end]).unwrap()));
                pos = end;
                continue;
            } else {
                unreachable!();
            }
        }
    }
}
impl<'a> fmt::Debug for Name<'a> {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_tuple("Name")
        .field(&format!("{}", self))
        .finish()
    }
}

#[cfg(test)]
mod test {
    use Error;
    use Name;

    #[test]
    fn parse_badpointer_same_offset() {
        // A buffer where an offset points to itself,
        // which is a bad compression pointer.
        let same_offset = vec![192, 2, 192, 2];
        let is_match = matches!(Name::scan(&same_offset, &same_offset),
                                Err(Error::BadPointer));

        assert!(is_match);
    }

    #[test]
    fn parse_badpointer_forward_offset() {
        // A buffer where the offsets points back to each other which causes
        // infinite recursion if never checked, a bad compression pointer.
        let forwards_offset = vec![192, 2, 192, 4, 192, 2];
        let is_match = matches!(Name::scan(&forwards_offset, &forwards_offset),
                                Err(Error::BadPointer));

        assert!(is_match);
    }

    #[test]
    fn nested_names() {
        // A buffer where an offset points to itself, a bad compression pointer.
        let buf = b"\x02xx\x00\x02yy\xc0\x00\x02zz\xc0\x04";

        assert_eq!(Name::scan(&buf[..], buf).unwrap().to_string(),
            "xx");
        assert_eq!(Name::scan(&buf[..], buf).unwrap().labels,
            b"\x02xx\x00");
        assert_eq!(Name::scan(&buf[4..], buf).unwrap().to_string(),
            "yy.xx");
        assert_eq!(Name::scan(&buf[4..], buf).unwrap().labels,
            b"\x02yy\xc0\x00");
        assert_eq!(Name::scan(&buf[9..], buf).unwrap().to_string(),
            "zz.yy.xx");
        assert_eq!(Name::scan(&buf[9..], buf).unwrap().labels,
            b"\x02zz\xc0\x04");
    }
}
