# Copyright (C) 2007-2012 LuaDist.
# Submitted by David Manura, Peter Drahos
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
# Please note that the package source code is licensed under its own license.

# warning: compilation options might not be optical

project ( scite CXX )
cmake_minimum_required ( VERSION 2.8 )
include ( cmake/dist.cmake )

find_package ( Lua REQUIRED )
find_library ( SCINTILLA_LIBRARY NAMES scintilla )
find_path ( SCINTILLA_INCLUDE_DIR Scintilla.h )

include_directories ( ${SCINTILLA_INCLUDE_DIR} ${LUA_INCLUDE_DIR} include src )
file ( GLOB SRC_SCITE src/*.cxx )
file ( GLOB PROPERTIES src/*.properties )

if ( WIN32 )
  include_directories ( win32 )
  add_definitions ( -DUNICODE -D_UNICODE )
  file ( GLOB SRC_WIN32 win32/*.cxx )
  list ( APPEND SRC_SCITE ${SRC_WIN32} win32/SciTERes.rc )
  set ( LIB -lmsimg32 -lcomctl32 -limm32 -lole32 -luuid -luxtheme )
  set ( EXE_TYPE WIN32 )
endif ()

add_executable ( scite ${EXE_TYPE} ${SRC_SCITE} )
target_link_libraries ( scite ${SCINTILLA_LIBRARY} ${LUA_LIBRARIES} ${LIB} )

install_executable ( scite )

# Install properties
install ( FILES SciTE.properties DESTINATION ${INSTALL_BIN} COMPONENT Data )
install_data ( ${PROPERTIES} INTO properties )
install_data ( scripts/ INTO scripts )
install_doc ( doc/ )
install_data ( README License.txt )
