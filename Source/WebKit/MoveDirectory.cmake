if (IS_DIRECTORY "${SRC}")
    file(REMOVE_RECURSE "${DST}")
    file(RENAME "${SRC}" "${DST}")
endif ()
