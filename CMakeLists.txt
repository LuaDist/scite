# Copyright (C) 2007-2009 LuaDist.
# Submitted by David Manura
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
# Please note that the package source code is licensed under its own license.

# warning: compilation options might not be optical

PROJECT ( scite )
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
INCLUDE(dist.cmake )

ADD_SUBDIRECTORY(scintilla)
ADD_SUBDIRECTORY(scite)
