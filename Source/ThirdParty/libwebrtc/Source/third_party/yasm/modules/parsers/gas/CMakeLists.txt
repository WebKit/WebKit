YASM_RE2C(
    ${CMAKE_CURRENT_SOURCE_DIR}/parsers/gas/gas-token.re
    ${CMAKE_CURRENT_BINARY_DIR}/gas-token.c
    -b
    )

YASM_ADD_MODULE(parser_gas
    parsers/gas/gas-parser.c
    parsers/gas/gas-parse.c
    parsers/gas/gas-parse-intel.c
    gas-token.c
    )
