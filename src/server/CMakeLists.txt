find_package(Boost 1.83 REQUIRED CONFIG)
find_package(Threads REQUIRED)

# All of the server except main(). The tests link this library too, so they
# exercise exactly the same code as the real server.
add_library(chess_server_lib STATIC
    config.cpp
    http_routes.cpp
    http_session.cpp
    listener.cpp
)
target_include_directories(chess_server_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(chess_server_lib PUBLIC chess::core Boost::headers Threads::Threads)
chess_set_warnings(chess_server_lib)

add_executable(chess_server main.cpp)
target_link_libraries(chess_server PRIVATE chess_server_lib)
chess_set_warnings(chess_server)

# deploy/Dockerfile.server copies the installed binary from /install/bin/chess_server.
install(TARGETS chess_server RUNTIME DESTINATION bin)
