/* The value of LINK_SIZE determines the number of bytes used to store links as
offsets within the compiled regex. The default is 2, which allows for compiled
patterns up to 64K long. This covers the vast majority of cases. However, PCRE
can also be compiled to use 3 or 4 bytes instead. This allows for longer
patterns in extreme cases. On systems that support it, "configure" can be used
to override this default. */

#define LINK_SIZE   2

/* The value of MATCH_LIMIT determines the default number of times the internal
match() function can be called during a single execution of pcre_exec(). There
is a runtime interface for setting a different limit. The limit exists in order
to catch runaway regular expressions that take for ever to determine that they
do not match. The default is set very large so that it does not accidentally
catch legitimate cases. On systems that support it, "configure" can be used to
override this default default. */

#define MATCH_LIMIT 10000000

/* The above limit applies to all calls of match(), whether or not they
increase the recursion depth. In some environments it is desirable to limit the
depth of recursive calls of match() more strictly, in order to restrict the
maximum amount of stack (or heap, if NO_RECURSE is defined) that is used. The
value of MATCH_LIMIT_RECURSION applies only to recursive calls of match(). To
have any useful effect, it must be less than the value of MATCH_LIMIT. There is
a runtime method for setting a different limit. On systems that support it,
"configure" can be used to override this default default. */

#define MATCH_LIMIT_RECURSION MATCH_LIMIT

/* End */
