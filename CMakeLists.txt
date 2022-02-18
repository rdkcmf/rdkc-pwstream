##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2022 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
cmake_minimum_required(VERSION 1.6.3)
PROJECT(pwstream)

SET(LIB_TYPE SHARED)

FILE(GLOB SOURCES
        "*.c*"
     )

INCLUDE_DIRECTORIES(".")

ADD_LIBRARY(${PROJECT_NAME} ${LIB_TYPE} ${SOURCES} )

install(FILES "pwstream.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

IF(${LIB_TYPE} MATCHES "SHARED")
    TARGET_LINK_LIBRARIES(${PROJECT_NAME}
    pipewire-0.3
    rdkloggers
    )

    INSTALL(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
ENDIF(${LIB_TYPE} MATCHES "SHARED")
