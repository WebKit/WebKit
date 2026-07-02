# Capstone Disassembly Engine
# Common functions used by Makefile & tests/Makefile

define compile
	$(ifeq ($(MACOS_UNIVERSAL),no),
		@$(CC) -MM -MP -MT $@ -MT $(@:.o=.d) $(CFLAGS) $< > $(@:.o=.d)
	)
	${CC} ${CFLAGS} -c $< -o $@
endef


define log
	@printf "  %-7s %s\n" "$(1)" "$(2)"
endef

