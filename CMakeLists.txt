cmake_minimum_required(VERSION 2.8.11)

project(xpjs CXX)
set (xpjs_VERSION_MAJOS 0)
set (xpjs_VERSION_MINOR 1)



# set XPlane SDK Directory
set(XPSDKDIR ../SDK)
set(MOZJSDIR_INC /usr/include/mozjs-24)
set(JSRDBGDIR_INC jsrdbg/public)
set(JSRDBGDIR_LIB jsrdbg/inst/lib)

# compiles options
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wextra -std=c++11 -Wall -O2 -fPIC")

#Bring the headers, such as Student.h into the project
include_directories(inc)
foreach (dir Widgets Wrappers XPLM)
  include_directories ("${XPSDKDIR}/CHeaders/${dir}")
endforeach ()
include_directories(${MOZJSDIR_INC})
include_directories(${JSRDBGDIR_INC})


#link directories
link_directories(${JSRDBGDIR_LIB})

#add definitions
add_definitions(-DAPL=0 -DIBM=0 -DLIN=1)


# project files
file(GLOB SOURCES "src/*.cpp")
file(GLOB INCLUDES "inc/*.h")

set(ALL_SRC ${SOURCES} ${INCLUDES})
LIST(SORT ALL_SRC)

SOURCE_GROUP(src FILES ${ALL_SRC})

# executable
add_library(xpjs SHARED ${ALL_SRC})


target_link_libraries (xpjs dl pthread mozjs-24 libjsrdbg.a)

set_target_properties(xpjs PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic -nodefaultlibs -undefined_warning -fPIC -fvisibility=hidden")

