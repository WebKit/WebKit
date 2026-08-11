from typing import Any, Optional, Mapping, MutableMapping, Union
from webdriver.bidi.undefined import UNDEFINED, Undefined

from ._module import BidiModule, command


class DigitalCredentials(BidiModule):
    @command
    def set_virtual_wallet_behavior(
        self,
        action: Union[Optional[str], Undefined] = UNDEFINED,
        context: Union[Optional[str], Undefined] = UNDEFINED,
        protocol: Union[Optional[str], Undefined] = UNDEFINED,
        response: Union[Optional[Mapping[str, Any]], Undefined] = UNDEFINED,
    ) -> Mapping[str, Any]:
        params: MutableMapping[str, Any] = {
            "action": action,
            "context": context,
            "protocol": protocol,
            "response": response,
        }
        return params
