project(GraphLab)

subdirs(
 engine
 graph
 logger
 monitoring
 parallel
 schedulers
 serialization
 scope
 shared_data
 tasks
 util
 extern)
  

add_library(graphlab STATIC
  logger/logger.cpp
  factors/table_factor.cpp
  factors/binary_factor.cpp
  factors/unary_factor.cpp
  parallel/pthread_tools.cpp
  serialization/iarchive.cpp
  serialization/oarchive.cpp
  util/timer.cpp
  util/generics/any.cpp
  util/command_line_options.cpp)



target_link_libraries(graphlab graphlab_metis)


INSTALL(TARGETS graphlab ARCHIVE DESTINATION lib)


