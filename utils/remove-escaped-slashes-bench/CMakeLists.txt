add_executable(remove-escaped-slashes-bench remove-escaped-slashes-bench.cpp)
target_link_libraries(remove-escaped-slashes-bench PRIVATE Poco::JSON Poco::Foundation)
