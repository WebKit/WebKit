import asyncio
import io
import logging
import os
import re
import struct
import urllib.parse
import traceback
from typing import Any, Dict, Iterator, List, Optional, Tuple

from aioquic.asyncio import QuicConnectionProtocol, serve
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.connection import END_STATES
from aioquic.quic.events import StreamDataReceived, QuicEvent
from aioquic.tls import SessionTicket

SERVER_NAME = 'aioquic-transport'
logger = logging.getLogger(__name__)

handlers_path = ''


class EventHandler:
    def __init__(self, connection: QuicConnectionProtocol, global_dict: Dict[str, Any]):
        self.connection = connection
        self.global_dict = global_dict

    def handle_client_indication(
            self,
            origin: str,
            query: Dict[str, str]) -> None:
        name = 'handle_client_indication'
        if name in self.global_dict:
            self.global_dict[name](self.connection, origin, query)

    def handle_event(self, event: QuicEvent) -> None:
        name = 'handle_event'
        if name in self.global_dict:
            self.global_dict[name](self.connection, event)


class QuicTransportProtocol(QuicConnectionProtocol):  # type: ignore
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.pending_events: List[QuicEvent] = []
        self.client_indication_finished = False
        self.client_indication_data = b''
        self.handler: Optional[EventHandler] = None

    def quic_event_received(self, event: QuicEvent) -> None:
        prefix = '!!'
        logger.info('QUIC event: %s', type(event))
        try:
            if (not self.client_indication_finished and
                    isinstance(event, StreamDataReceived) and
                    event.stream_id == 2):
                # client indication process
                prefix = 'Client indication error: '
                self.client_indication_data += event.data
                if len(self.client_indication_data) > 65535:
                    raise Exception('too large data for client indication')
                if event.end_stream:
                    self.process_client_indication()
                    assert self.handler is not None
                    if self.is_closing_or_closed():
                        return
                    prefix = 'Event handling Error: '
                    for e in self.pending_events:
                        self.handler.handle_event(e)
                    self.pending_events.clear()
            elif not self.client_indication_finished:
                self.pending_events.append(event)
            elif self.handler is not None:
                prefix = 'Event handling Error: '
                self.handler.handle_event(event)
        except Exception as e:
            self.handler = None
            logger.warn(prefix + str(e))
            traceback.print_exc()
            self.close()

    def parse_client_indication(self, bs: io.BytesIO) -> Iterator[Tuple[int, bytes]]:
        while True:
            key_b = bs.read(2)
            if len(key_b) == 0:
                return
            length_b = bs.read(2)
            if len(key_b) != 2:
                raise Exception('failed to get "Key" field')
            if len(length_b) != 2:
                raise Exception('failed to get "Length" field')
            key = struct.unpack('!H', key_b)[0]
            length = struct.unpack('!H', length_b)[0]
            value = bs.read(length)
            if len(value) != length:
                raise Exception('truncated "Value" field')
            yield (key, value)

    def process_client_indication(self) -> None:
        origin = None
        origin_string = None
        path = None
        path_string = None
        KEY_ORIGIN = 0
        KEY_PATH = 1
        for (key, value) in self.parse_client_indication(
                io.BytesIO(self.client_indication_data)):
            if key == KEY_ORIGIN:
                origin_string = value.decode()
                origin = urllib.parse.urlparse(origin_string)
            elif key == KEY_PATH:
                path_string = value.decode()
                path = urllib.parse.urlparse(path_string)
            else:
                # We must ignore unrecognized fields.
                pass
        logger.info('origin = %s, path = %s', origin_string, path_string)
        if origin is None:
            raise Exception('No origin is given')
        assert origin_string is not None
        if path is None:
            raise Exception('No path is given')
        assert path_string is not None
        if origin.scheme != 'https' and origin.scheme != 'http':
            raise Exception('Invalid origin: %s' % origin_string)
        if origin.netloc == '':
            raise Exception('Invalid origin: %s' % origin_string)

        # To make the situation simple we accept only simple path strings.
        m = re.compile(r'^/([a-zA-Z0-9\._\-]+)$').match(path.path)
        if m is None:
            raise Exception('Invalid path: %s' % path_string)

        handler_name = m.group(1)
        query = dict(urllib.parse.parse_qsl(path.query))
        self.handler = self.create_event_handler(handler_name)
        self.handler.handle_client_indication(origin_string, query)
        if self.is_closing_or_closed():
            return
        self.client_indication_finished = True
        logger.info('Client indication finished')

    def create_event_handler(self, handler_name: str) -> EventHandler:
        global_dict: Dict[str, Any] = {}
        with open(handlers_path + '/' + handler_name) as f:
            exec(f.read(), global_dict)
        return EventHandler(self, global_dict)

    def is_closing_or_closed(self) -> bool:
        if self._quic._close_pending:
            return True
        if self._quic._state in END_STATES:
            return True
        return False


class SessionTicketStore:
    '''
    Simple in-memory store for session tickets.
    '''

    def __init__(self) -> None:
        self.tickets: Dict[bytes, SessionTicket] = {}

    def add(self, ticket: SessionTicket) -> None:
        self.tickets[ticket.ticket] = ticket

    def pop(self, label: bytes) -> Optional[SessionTicket]:
        return self.tickets.pop(label, None)


def start(**kwargs: Any) -> None:
    configuration = QuicConfiguration(
        alpn_protocols=['wq-vvv-01'] + ['siduck'],
        is_client=False,
        max_datagram_frame_size=65536,
    )

    global handlers_path
    handlers_path = os.path.abspath(os.path.expanduser(
        kwargs['handlers_path']))
    logger.info('port = %s', kwargs['port'])
    logger.info('handlers path = %s', handlers_path)

    # load SSL certificate and key
    configuration.load_cert_chain(kwargs['certificate'], kwargs['private_key'])

    ticket_store = SessionTicketStore()

    loop = asyncio.get_event_loop()
    loop.run_until_complete(
        serve(
            kwargs['host'],
            kwargs['port'],
            configuration=configuration,
            create_protocol=QuicTransportProtocol,
            session_ticket_fetcher=ticket_store.pop,
            session_ticket_handler=ticket_store.add,
        )
    )
    try:
        loop.run_forever()
    except KeyboardInterrupt:
        pass
