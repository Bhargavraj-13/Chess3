# Turns on strict compiler warnings for one of our own targets.
# Never call this on third-party targets such as Catch2.
function(chess_set_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
    )
endfunction()
