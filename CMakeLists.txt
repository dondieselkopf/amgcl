cmake_minimum_required(VERSION 2.8)
project(amgcl)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#----------------------------------------------------------------------------
# Find Boost
#----------------------------------------------------------------------------
option(
    BOOST_TEST_DYN_LINK
    "Link tests against dynamic version of boost unittest library"
    ON
    )

if (WIN32)
    set(Boost_USE_STATIC_LIBS ON)
else ()
  if (BOOST_TEST_DYN_LINK)
    add_definitions(-DBOOST_TEST_DYN_LINK)
  endif ()
endif ()

set(BOOST_COMPONENTS
    chrono
    program_options
    system
    unit_test_framework
    )

find_package(Boost COMPONENTS ${BOOST_COMPONENTS})
include_directories( ${Boost_INCLUDE_DIRS} )

#----------------------------------------------------------------------------
# Find OpenMP
#----------------------------------------------------------------------------
find_package(OpenMP)

if (OpenMP_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif ()

#----------------------------------------------------------------------------
# Find Eigen
#----------------------------------------------------------------------------
find_path(EIGEN_INCLUDE Eigen/SparseCore PATH_SUFFIXES eigen3)
if (EIGEN_INCLUDE)
    include_directories(${EIGEN_INCLUDE})
    add_definitions(-DAMGCL_HAVE_EIGEN)
endif()

#----------------------------------------------------------------------------
# Find VexCL
#----------------------------------------------------------------------------
set(VEXCL_ROOT $ENV{VEXCL_ROOT} CACHE STRING "VexCL root")
if (VEXCL_ROOT)
    include_directories( ${VEXCL_ROOT} )
    add_definitions(-DAMGCL_HAVE_VEXCL)

    set(VEXCL_BACKEND "OpenCL" CACHE STRING "Select VexCL backend (OpenCL/CUDA)")
    set_property(CACHE VEXCL_BACKEND PROPERTY STRINGS "OpenCL" "CUDA")

    #----------------------------------------------------------------------------
    # Find VexCL backend
    #----------------------------------------------------------------------------
    if ("${VEXCL_BACKEND}" STREQUAL "OpenCL")
        set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
        find_package(OpenCL REQUIRED)
        include_directories( ${OPENCL_INCLUDE_DIRS} )
        set(VEXCL_BACKEND_LIBRARIES ${OPENCL_LIBRARIES})
        add_definitions(-DVEXCL_BACKEND_OPENCL)
    elseif ("${VEXCL_BACKEND}" STREQUAL "CUDA")
        find_package(CUDA REQUIRED)
        include_directories( ${CUDA_INCLUDE_DIRS} )
        set(VEXCL_BACKEND_LIBRARIES ${CUDA_CUDA_LIBRARY})
        add_definitions(-DVEXCL_BACKEND_CUDA)
    endif()
endif()

#----------------------------------------------------------------------------
# Protect Visual Studio from itself
#----------------------------------------------------------------------------
if (WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_VARIADIC_MAX=10)
    add_definitions(/bigobj)
endif ()

#----------------------------------------------------------------------------
# Enable C++11 support, set compilation flags
#----------------------------------------------------------------------------
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wsign-compare -Wtype-limits -Wuninitialized -Wunused-parameter -Wunused-but-set-parameter -Wno-comment -Wno-type-limits -Wno-strict-aliasing")
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wsign-compare -Wtype-limits -Wuninitialized -Wunused-parameter -Wno-comment -Wno-tautological-compare")

    option(USE_LIBCPP "Use libc++ with Clang" OFF)
    if (USE_LIBCPP)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif ()
endif ()

#----------------------------------------------------------------------------
# Tests
#----------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
add_subdirectory(examples)