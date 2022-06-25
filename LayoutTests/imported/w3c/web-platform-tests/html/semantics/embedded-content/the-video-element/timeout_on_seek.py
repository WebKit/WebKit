
import os
import re
from urllib.parse import parse_qs
from wptserve.utils import isomorphic_decode, isomorphic_encode

def parse_range(header_value, file_size):
    if header_value is None:
        # HTTP Range header range end is inclusive
        return 0, file_size - 1

    match = re.match("bytes=(\d*)-(\d*)", header_value)
    start = int(match.group(1)) if match.group(1).strip() != "" else 0
    last = int(match.group(2)) if match.group(2).strip() != "" else file_size - 1
    return start, last

def main(request, response):
    file_extension = parse_qs(request.url_parts.query)["extension"][0]
    with open("media/movie_300." + file_extension, "rb") as f:
        f.seek(0, os.SEEK_END)
        file_size = f.tell()

        range_header = isomorphic_decode(request.headers.get(b"range"))
        req_start, req_last = parse_range(range_header, file_size)
        f.seek(req_start, os.SEEK_SET)

        response.add_required_headers = False
        response.writer.write_status(206 if range_header else 200)
        response.writer.write_header(b"Accept-Ranges", b"bytes")
        response.writer.write_header(b"Content-Type", b"video/mp4")
        if range_header:
            response.writer.write_header(b"Content-Range", b"bytes %d-%d/%d" %
                    (req_start, req_last, file_size))
        response.writer.write_header(b"Content-Length", isomorphic_encode(str(req_last - req_start + 1)))
        response.writer.end_headers()

        gap_start = int(file_size * 0.5)
        gap_last = int(file_size * 0.95)

        if gap_start < req_start < gap_last:
            # If the start position is part of the gap, don't send any data
            return

        if req_start < gap_start:
            # If the position is before of the gap, only send data until the
            # gap is reached
            req_last = min(req_last, gap_start)

        size = req_last - req_start + 1
        response.writer.write(f.read(size))
