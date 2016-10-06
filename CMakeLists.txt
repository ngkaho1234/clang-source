cmake_minimum_required(VERSION 2.6)

project (clang-source)
option(USE_CLANG "build application with clang" ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_BUILD_TYPE Debug)

find_package(LibClang REQUIRED)
find_package(Msgpack REQUIRED)
find_package(SQLite3 REQUIRED)

set(COMMON clang-source-db.cxx)

include_directories(
    SYSTEM
    ${LibClang_INCLUDE_DIRS}
    ${MSGPACK_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)

if (USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
    add_definitions(-DCLANG_SOURCE_DB_TEST)
endif (USE_CLANG)

#add_executable(clang-source-server ${COMMON} clang-source-server.c)
#target_link_libraries(clang-source-server
    #${LibClang_LIBRARIES}
    #${MSGPACK_LIBRARIES}
    #${SQLITE3_LIBRARIES}
#)
add_executable(clang-source-db ${COMMON})
target_link_libraries(clang-source-db
    ${LibClang_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    ${SQLITE3_LIBRARIES}
)
