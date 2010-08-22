# Force the use of gcc-4.2 as the default compiler on macs
IF(APPLE)
  SET(CMAKE_C_COMPILER "gcc-4.2")	
  SET(CMAKE_CXX_COMPILER "c++-4.2")
ENDIF(APPLE)


project(GraphLab)

cmake_minimum_required(VERSION 2.4)

set(CMAKE_MODULE_PATH ${GraphLab_SOURCE_DIR}/cmake)

#set(CROSS_COMPILE_32_BIT 1)




# Use the Boost libraries
FIND_PACKAGE(Boost 1.37 COMPONENTS 
  program_options
  REQUIRED)


if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0005 NEW)
endif(COMMAND cmake_policy)

# cmake_policy(SET CMP0014 OLD)


# architecture detection
# check 64 bit
if(CMAKE_SIZEOF_VOID_P MATCHES 4)
  set( HAVE_64_BIT 0 )
else(CMAKE_SIZEOF_VOID_P MATCHES 4)
  set( HAVE_64_BIT 1 )
endif(CMAKE_SIZEOF_VOID_P MATCHES 4)

if(CROSS_COMPILE_32_BIT MATCHES 1)
set(HAVE_64_BIT 0)
endif(CROSS_COMPILE_32_BIT MATCHES 1)

# Specify the subdirectories for this root directory This will be
# used for the class branch, not the lab branch: subdirs(src examples
# EXCLUDE_FROM_ALL tests timings projects) (Remove EXCLUDE_FROM_ALL
# for lab branch.)
subdirs(src apps)


# add random environment variable definitions
# deal with Boost time shtuff: we want nanoseconds!
# add_definitions(-DBOOST_DATE_TIME_POSIX_TIME_STD_CONFIG)
# add_definitions(-DBOOST_ALL_DYN_LINK)

add_definitions(-D_SCL_SECURE_NO_WARNINGS) # disable checked_iterator warnings
add_definitions(-D_CRT_SECURE_NO_WARNINGS) # disable fopen warnings
add_definitions(-D_SECURE_SCL=0)  # necessary for release builds of MSVC

# Remove tracing by default
# add_definitions(-DTRACE_OFF)

## Here we use our cmake tools
#set(CMAKE_MODULE_PATH ${PRL_SOURCE_DIR}/cmake)

set(BUILD_SHARED_LIBS ON)

# set include path for this and all subdirs
include_directories(
  ${GraphLab_SOURCE_DIR}/src
  ${Boost_INCLUDE_DIR}
  )

# add boost to link directories and link libraries
link_directories(
  ${Boost_LIBRARY_DIRS}
  )
link_libraries(${Boost_LIBRARIES})



set(GraphLab_LIBRARIES graphlab)


# Add posix threads on non-windows systems
if(NOT ${CMAKE_SYSTEM} MATCHES Windows)
  link_libraries(pthread)   
endif(NOT ${CMAKE_SYSTEM} MATCHES Windows)

# Flags for the debug and release mode
if(CMAKE_COMPILER_IS_GNUCXX)
  
  ### Set the debug flags ----------------------------------------------------->
  # Note: -fkeep-inline-functions significantly slows down the compilation
  set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb -O0 -Wall -Winit-self  ") 
                                   #-fno-implicit-templates")

  ### Set the release flags --------------------------------------------------->
  if (CROSS_COMPILE_32_BIT MATCHES 1)

    set(CMAKE_CXX_FLAGS_RELEASE 
      "-m32 -O3 -Wall -Wextra -g -mfpmath=sse -msse2 -funroll-loops -fprefetch-loop-arrays ")
    set(CMAKE_C_FLAGS_RELEASE 
      "-m32 -O3 -Wall -Wextra -g -mfpmath=sse -msse2 -funroll-loops -fprefetch-loop-arrays ")

  else(CROSS_COMPILE_32_BIT MATCHES 1)
    
    # Set the C compiler flags in release mode on 64bit systems
    set(CMAKE_C_FLAGS_RELEASE 
      "-O3 -Wall -g -mfpmath=sse -msse2 -funroll-loops -fprefetch-loop-arrays ")
    if (COMPILE_PROFILING MATCHES 1) 
      set(CMAKE_CXX_FLAGS_RELEASE 
        "-O2 -Wall -g -mfpmath=sse -msse2 -fprefetch-loop-arrays -pg ")
    else(COMPILE_PROFILING MATCHES 1)
      set(CMAKE_CXX_FLAGS_RELEASE 
        "-O3 -Wall -g -mfpmath=sse -msse2 -funroll-loops -fprefetch-loop-arrays ")
    endif(COMPILE_PROFILING MATCHES 1)

  endif(CROSS_COMPILE_32_BIT MATCHES 1)

# Mex setup
 set(CMAKE_CXX_FLAGS_MEX  "${CMAKE_CXX_FLAGS_RELEASE} -D_GNU_SOURCE -fPIC -fno-omit-frame-pointer -pthread ")
 set(CMAKE_C_FLAGS_MEX  "${CMAKE_C_FLAGS_RELEASE}   -D_GNU_SOURCE  -fexceptions -fPIC -fno-omit-frame-pointer -pthread ")


if (CMAKE_BUILD_TYPE MATCHES "Release")
  add_definitions(-DCOMPILEFLAGS=${CMAKE_CXX_FLAGS_RELEASE})
elseif(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DCOMPILEFLAGS=${CMAKE_CXX_FLAGS_DEBUG})
elseif(CMAKE_BUILD_TYPE MATCHES "Mex")
  message(STATUS "Mex CXX" ${CMAKE_CXX_FLAGS_MEX})
  message(STATUS "Mex C" ${CMAKE_C_FLAGS_MEX})

  add_definitions(-DCOMPILEFLAGS=${CMAKE_CXX_FLAGS_MEX})
endif()


add_definitions(-DJU_64BIT)

endif(CMAKE_COMPILER_IS_GNUCXX)

# uncomment the following line to get CMake variables to print to screen
# include(CMakePrintSystemInformation)






## Installation rules
## ============================================================
INSTALL(DIRECTORY src/  
  DESTINATION include
  FILES_MATCHING PATTERN "*.hpp"  PATTERN "*.h" 
  PATTERN ".svn" EXCLUDE
)


# Install target graphlab moved to after definition


