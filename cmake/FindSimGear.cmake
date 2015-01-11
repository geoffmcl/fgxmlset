# Locate SimGear
# This module defines

# SIMGEAR_CORE_LIBRARIES, a list of the core static libraries
# SIMGEAR_LIBRARIES, a list of all the static libraries (core + scene)
# SIMGEAR_FOUND, if false, do not try to link to SimGear
# SIMGEAR_INCLUDE_DIRS, where to find the headers
#
# $SIMGEAR_DIR is an environment variable that would
# correspond to the ./configure --prefix=$SIMGEAR_DIR
# used in building SimGear.
#
# Created by James Turner. This was influenced by the FindOpenAL.cmake module.

#=============================================================================
# Copyright 2005-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)

include(SelectLibraryConfigurations)

macro(DBG_MSG _MSG)
    if (SG_DBG_FINDING)
        message(STATUS "DBG: ${_MSG}")
    endif ()
endmacro()

macro(find_sg_library libName varName libs)
    set(libVarName "${varName}_LIBRARY")
    # do not cache the library check
    unset(${libVarName}_DEBUG CACHE)
    unset(${libVarName}_RELEASE CACHE)

    FIND_LIBRARY(${libVarName}_DEBUG
      NAMES ${libName}${CMAKE_DEBUG_POSTFIX}
      HINTS $ENV{SIMGEAR_DIR}
      PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR} lib libs64 libs libs/Win32 libs/Win64
      PATHS
      ${ADDITIONAL_LIBRARY_PATHS}
    )
    FIND_LIBRARY(${libVarName}_RELEASE
      NAMES ${libName}${CMAKE_RELEASE_POSTFIX}
      HINTS $ENV{SIMGEAR_DIR}
      PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR} lib libs64 libs libs/Win32 libs/Win64
      PATHS
      ${ADDITIONAL_LIBRARY_PATHS}
    )
    
    DBG_MSG("before: Simgear REL ${${libVarName}_RELEASE} ")
    DBG_MSG("before: Simgear DBG ${${libVarName}_DEBUG} ")
    
    select_library_configurations( ${varName} )

    DBG_MSG("after:Simgear REL ${${libVarName}_RELEASE} ")
    DBG_MSG("after:Simgear DBG ${${libVarName}_DEBUG} ")

    set(componentLibRelease ${${libVarName}_RELEASE})
    DBG_MSG("Simgear ${libVarName}_RELEASE ${componentLibRelease}")
    set(componentLibDebug ${${libVarName}_DEBUG})
    DBG_MSG("Simgear ${libVarName}_DEBUG ${componentLibDebug}")
    
    if (componentLibRelease AND componentLibDebug)
        list(APPEND ${libs} optimized ${componentLibRelease} debug ${componentLibDebug})
    else ()
        if (componentLibRelease)
            list(APPEND ${libs} ${componentLibRelease})
        endif ()
    endif ()
    #if (NOT ${libVarName}_DEBUG)
    #    if (NOT ${libVarName}_RELEASE)
    #        DBG_MSG("found ${componentLib}")
    #        list(APPEND ${libs} ${componentLibRelease})
    #    endif()
    #else()
    #    list(APPEND ${libs} optimized ${componentLibRelease} debug ${componentLibDebug})
    #endif()
endmacro()

FIND_PATH(SIMGEAR_INCLUDE_DIR simgear/math/SGMath.hxx
  HINTS $ENV{SIMGEAR_DIR}
  PATH_SUFFIXES include
  PATHS
  ${ADDITIONAL_LIBRARY_PATHS}
)

# make sure the simgear include directory exists
if (NOT SIMGEAR_INCLUDE_DIR)
    message(STATUS "Cannot find SimGear includes! (Forgot 'make install' for SimGear?) "
            "Compile & INSTALL SimGear before configuring FlightGear. "
            "When using non-standard locations, use 'SIMGEAR_DIR' to configure the SimGear location.")
else (NOT SIMGEAR_INCLUDE_DIR)

message(STATUS "SimGear include directory: ${SIMGEAR_INCLUDE_DIR}")

# read the simgear version header file, get the version
file(READ ${SIMGEAR_INCLUDE_DIR}/simgear/version.h SG_VERSION_FILE)

# make sure the simgear/version.h header exists
if (NOT SG_VERSION_FILE)
    message(STATUS "Found SimGear, but it does not contain a simgear/version.h include! "
            "SimGear installation is incomplete or mismatching.")
else (NOT SG_VERSION_FILE)

string(STRIP "${SG_VERSION_FILE}" SIMGEAR_DEFINE)
string(REPLACE "#define SIMGEAR_VERSION " "" SIMGEAR_VERSION "${SIMGEAR_DEFINE}")

if (NOT SIMGEAR_VERSION)
    message(STATUS "Unable to find SimGear or simgear/version.h does not exist/is invalid. "
            "Make sure you have installed the SimGear ${SimGear_FIND_VERSION} includes. "
            "When using non-standard locations, please use 'SIMGEAR_DIR' "
            "to select the SimGear library location to be used.")
else (NOT SIMGEAR_VERSION)

message(STATUS "found SimGear version: ${SIMGEAR_VERSION}")
#message(STATUS "found SimGear version: ${SIMGEAR_VERSION} (needed ${SimGear_FIND_VERSION})")
#if(NOT "${SIMGEAR_VERSION}" EQUAL "${SimGear_FIND_VERSION}")
#    message(FATAL_ERROR "You have installed a mismatching SimGear version ${SIMGEAR_VERSION} "
#            "instead of ${SimGear_FIND_VERSION} as required by FlightGear. "
#            "When using multiple SimGear installations, please use 'SIMGEAR_DIR' "
#            "to select the SimGear library location to be used.")
#endif()

# dependent packages
# find_package(ZLIB REQUIRED)
# find_package(Threads REQUIRED)

if(SIMGEAR_SHARED)
    DBG_MSG("looking for shared Simgear libraries")

    find_sg_library(SimGearCore SIMGEAR_CORE SIMGEAR_CORE_LIBRARIES)
    find_sg_library(SimGearScene SIMGEAR_SCENE SIMGEAR_LIBRARIES)
 
    list(APPEND SIMGEAR_LIBRARIES ${SIMGEAR_CORE_LIBRARIES})
    set(SIMGEAR_CORE_LIBRARY_DEPENDENCIES "")
    set(SIMGEAR_SCENE_LIBRARY_DEPENDENCIES "")
    
    DBG_MSG("core lib ${SIMGEAR_CORE_LIBRARIES}")
    DBG_MSG("all libs ${SIMGEAR_LIBRARIES}")
else(SIMGEAR_SHARED)

    set(SIMGEAR_LIBRARIES "") # clear value
    set(SIMGEAR_CORE_LIBRARIES "") # clear value
    DBG_MSG("looking for static SimGear libraries")
    
    find_sg_library(SimGearCore SIMGEAR_CORE SIMGEAR_CORE_LIBRARIES)
    find_sg_library(SimGearScene SIMGEAR_SCENE SIMGEAR_LIBRARIES)

    # again link order matters - scene libraries depend on core ones
    list(APPEND SIMGEAR_LIBRARIES ${SIMGEAR_CORE_LIBRARIES})
    
    #set(SIMGEAR_CORE_LIBRARY_DEPENDENCIES
    #    ${CMAKE_THREAD_LIBS_INIT}
    #    ${ZLIB_LIBRARY}
    #    ${WINMM_LIBRARY})

    #set(SIMGEAR_SCENE_LIBRARY_DEPENDENCIES 
    #    ${OPENAL_LIBRARY})

    #if(APPLE)
    #    find_library(COCOA_LIBRARY Cocoa)
    #    list(APPEND SIMGEAR_CORE_LIBRARY_DEPENDENCIES ${COCOA_LIBRARY})
    #endif()

    #if(WIN32)
    #    list(APPEND SIMGEAR_CORE_LIBRARY_DEPENDENCIES ws2_32.lib)
    #endif(WIN32)

    #if(NOT MSVC)
    #    # basic timing routines on non windows systems, may be also cygwin?!
    #    check_library_exists(rt clock_gettime "" have_rt)
    #    if(have_rt)
    #        list(APPEND SIMGEAR_CORE_LIBRARY_DEPENDENCIES rt)
    #    endif(have_rt)
    #endif(NOT MSVC)
    DBG_MSG("core lib ${SIMGEAR_CORE_LIBRARIES}")
    DBG_MSG("all libs ${SIMGEAR_LIBRARIES}")
endif(SIMGEAR_SHARED)

if((NOT SIMGEAR_CORE_LIBRARIES) OR (NOT SIMGEAR_LIBRARIES))
    message(STATUS "Cannot find SimGear libraries! (Forgot 'make install' for SimGear?) "
            "Compile & INSTALL SimGear before configuring FlightGear. "
            "When using non-standard locations, use 'SIMGEAR_DIR' to configure the SimGear location.")
            
else((NOT SIMGEAR_CORE_LIBRARIES) OR (NOT SIMGEAR_LIBRARIES))
    ### message(STATUS "found SimGear libraries")

# now we've found SimGear, try test-compiling using its includes
if (RUN_SIMGEAR_TEST)
#####################################################################
include(CheckCXXSourceRuns)

set(SIMGEAR_INCLUDE_DIRS
  ${SIMGEAR_INCLUDE_DIR}
  ${SIMGEAR_INCLUDE_DIR}/simgear/3rdparty/utf8
)
SET(CMAKE_REQUIRED_INCLUDES ${SIMGEAR_INCLUDE_DIRS})

# clear cache, run a fresh compile test every time
unset(SIMGEAR_COMPILE_TEST CACHE)

# disable OSG dependencies for test-compiling
set(CMAKE_REQUIRED_DEFINITIONS "-DNO_OPENSCENEGRAPH_INTERFACE")
check_cxx_source_runs(
    "#include <cstdio>
    #include \"simgear/version.h\"
    #include \"simgear/math/SGMath.hxx\"

    #define xstr(s) str(s)
    #define str(s) #s

    #define MIN_MAJOR ${SimGear_FIND_VERSION_MAJOR}
    #define MIN_MINOR ${SimGear_FIND_VERSION_MINOR}
    #define MIN_MICRO ${SimGear_FIND_VERSION_PATCH}

    int main() {
        int major, minor, micro;

        /* printf(%d.%d.%d or greater, , MIN_MAJOR, MIN_MINOR, MIN_MICRO); */
        printf(\"found %s ... \", xstr(SIMGEAR_VERSION));

        sscanf( xstr(SIMGEAR_VERSION), \"%d.%d.%d\", &major, &minor, &micro );

        if ( (major != MIN_MAJOR) ||
             (minor != MIN_MINOR) ||
             (micro != MIN_MICRO) ) {
         return -1;
        }

        return 0;
    }
    "
    SIMGEAR_COMPILE_TEST)

if(NOT SIMGEAR_COMPILE_TEST)
    message(FATAL_ERROR "Oops, you have installed SimGear includes, however test compiling failed. "
            "Try removing 'CMakeCache.txt' and reconfigure with 'cmake'.")
endif()
unset(CMAKE_REQUIRED_DEFINITIONS)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SimGear DEFAULT_MSG
     SIMGEAR_LIBRARIES SIMGEAR_CORE_LIBRARIES SIMGEAR_INCLUDE_DIRS SIMGEAR_COMPILE_TEST)
#############################################################################
else ()
############################################################################
set( SIMGEAR_INCLUDE_DIRS ${SIMGEAR_INCLUDE_DIR} )
# list(REMOVE_DUPLICATES SIMGEAR_LIBRARIES)
# list(REMOVE_DUPLICATES SIMGEAR_CORE_LIBRARIES)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SimGear DEFAULT_MSG
     SIMGEAR_LIBRARIES SIMGEAR_CORE_LIBRARIES SIMGEAR_INCLUDE_DIRS)
############################################################################
endif ()

endif((NOT SIMGEAR_CORE_LIBRARIES) OR (NOT SIMGEAR_LIBRARIES))
endif (NOT SIMGEAR_VERSION)
endif (NOT SG_VERSION_FILE)
endif (NOT SIMGEAR_INCLUDE_DIR)

# eof
