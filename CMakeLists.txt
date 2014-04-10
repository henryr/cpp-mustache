cmake_minimum_required(VERSION 2.6)

# - Find rapidjson headers and lib.
# This module defines RAPIDJSON_INCLUDE_DIR, directory containing headers

set(RAPIDJSON_SEARCH_HEADER_PATHS
  ${CMAKE_SOURCE_DIR}/thirdparty/rapidjson/include/rapidjson
)

set(RAPIDJSON_INCLUDE_DIR
  ${CMAKE_SOURCE_DIR}/thirdparty/rapidjson/include
)

if (RAPIDJSON_INCLUDE_DIR)
  set(RAPIDJSON_FOUND TRUE)
else ()
  set(RAPIDJSON_FOUND FALSE)
endif ()

if (RAPIDJSON_FOUND)
  if (NOT RAPIDJSON_FIND_QUIETLY)
    message(STATUS "RapidJson headers found in: ${RAPIDJSON_INCLUDE_DIR}")
  endif ()
else ()
  message(STATUS "RapidJson headers NOT found.")
endif ()

mark_as_advanced(
  RAPIDJSON_INCLUDE_DIR
)

include_directories(SYSTEM ${RAPIDJSON_INCLUDE_DIR})

include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/thirdparty/gtest-1.7.0/include)
link_directories(${CMAKE_SOURCE_DIR}/thirdparty/gtest-1.7.0/mybuild/)

# add_executable(mustache-test mustache.cc)
# target_link_libraries(mustache boost_system boost_thread-mt)

add_executable(mustache-test mustache-tests.cc mustache.cc)
target_link_libraries(mustache-test boost_system boost_thread-mt gtest)

