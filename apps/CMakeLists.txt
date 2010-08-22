project(GraphLab)

#subdirs(
#  image_denoise
#  factor_graph_inference
#  gabp
#  pagerank
#  svm
#  shooting
#  demo
#)

link_libraries(${GraphLab_LIBRARIES})

macro(add_all_subdirectories retval curdir)
  file(GLOB sub-dir RELATIVE ${curdir} *)
  set(list_of_dirs "")
  foreach(dir ${sub-dir})
    if(IS_DIRECTORY ${curdir}/${dir})
    STRING(SUBSTRING ${dir} 0 1 firstchar)
        if(${firstchar} STREQUAL ".")
        else(${firstchar} STREQUAL ".")
          set(list_of_dirs ${list_of_dirs} ${dir})
          message(STATUS "Detected apps: " ${dir})
          add_subdirectory(${dir})
        endif()
    endif()
  endforeach()
  set(${retval} ${list_of_dirs})
endmacro()

add_all_subdirectories(retval, ${CMAKE_CURRENT_SOURCE_DIR})



