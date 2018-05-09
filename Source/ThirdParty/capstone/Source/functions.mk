# Capstone Disassembly Engine
# Common functions used by Makefile & tests/Makefile

define compile
	${CC} ${CFLAGS} -c $< -o $@
endef


define log
	@printf "  %-7s %s\n" "$(1)" "$(2)"
endef

