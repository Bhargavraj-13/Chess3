add_executable(chess_server main.cpp)
target_link_libraries(chess_server PRIVATE chess::core)
chess_set_warnings(chess_server)

# deploy/Dockerfile.server copies the installed binary from /install/bin/chess_server.
install(TARGETS chess_server RUNTIME DESTINATION bin)
