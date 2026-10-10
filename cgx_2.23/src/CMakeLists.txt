# --------------------------------------------------------------------  
#                          CALCULIX                                     
#                   - GRAPHICAL INTERFACE -                             
#                                                                       
#     A 3-dimensional pre- and post-processor for finite elements       
#              Copyright (C) 1996 Klaus Wittig                          
#                                                                       
#     This program is free software; you can redistribute it and/or     
#     modify it under the terms of the GNU General Public License as    
#     published by the Free Software Foundation; version 2 of           
#     the License.                                                      
#                                                                       
#     This program is distributed in the hope that it will be useful,   
#     but WITHOUT ANY WARRANTY; without even the implied warranty of    
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      
#     GNU General Public License for more details.                      
#                                                                       
#     You should have received a copy of the GNU General Public License 
#     along with this program; if not, write to the Free Software       
#     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.         
# --------------------------------------------------------------------  

# cgx 2.23 — CMake build (Step 1: Qt6 migration foundation)
# Replaces legacy Makefile + Makefile.inc. Behavior parity: same sources,
# same defines/includes, same link libraries, plus Qt6 linked for Step 2+.
# C sources need gnu17 (improveMesh.c uses K&R definitions rejected by GCC 15
# default gnu23). C++ sources use gnu++17.
cmake_minimum_required(VERSION 3.16)
project(cgx VERSION 2.23 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)          # -> gnu17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)        # -> gnu++17
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# --- location-independent setup --------------------------------------------
# Every project path below is derived from this file's own directory, so the
# tree can be renamed, moved or installed under any prefix without touching
# the build files (only system packages are located via find_package).
# Layout: <...>/cgx_X.XX/{src,examples,...} with libSNL as a sibling of the
# tree (upstream convention: <...>/libSNL/src), or vendored inside the tree.
get_filename_component(CGX_TREE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
# Autodetect whenever the cached value is empty or no longer valid, so a stale
# entry from a renamed/moved tree can never block the configure; a user-passed
# -DCGX_LIBSNL_DIR=... wins as long as it points at a real directory.
set(_cgx_libsnl "${CGX_LIBSNL_DIR}")
if(NOT _cgx_libsnl OR NOT IS_DIRECTORY "${_cgx_libsnl}")
  set(_cgx_libsnl "")
  foreach(_d "${CGX_TREE_DIR}/libSNL/src" "${CGX_TREE_DIR}/../libSNL/src")
    if(IS_DIRECTORY "${_d}")
      get_filename_component(_cgx_libsnl "${_d}" ABSOLUTE)
      break()
    endif()
  endforeach()
endif()
set(CGX_LIBSNL_DIR "${_cgx_libsnl}" CACHE PATH
    "Directory holding the libSNL sources (auto-detected: <tree>/libSNL/src or its upstream sibling)"
    FORCE)
if(NOT IS_DIRECTORY "${CGX_LIBSNL_DIR}")
  message(FATAL_ERROR
    "libSNL sources not found (searched: <tree>/libSNL/src and "
    "<parent-of-tree>/libSNL/src, i.e. the upstream sibling of the cgx tree).\n"
    "Restore libSNL next to the cgx tree, or pass "
    "-DCGX_LIBSNL_DIR=<path-to-libSNL/src>.")
endif()

# Example used by the headless part of the build/install check (see
# cmake/RunCgxCheck.cmake). Point it elsewhere with -DCGX_CHECK_EXAMPLE=...
set(CGX_CHECK_EXAMPLE "${CGX_TREE_DIR}/examples/basic/disc.fbd" CACHE FILEPATH
    "Example .fbd used by the cgx build/install check")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
find_package(OpenGL REQUIRED)
if(NOT OPENGL_GLU_FOUND)
  message(FATAL_ERROR
    "GLU not found. cgx needs GL/glu.h and libGLU (Debian/Ubuntu: "
    "sudo apt install libglu1-mesa-dev).")
endif()
find_package(Threads REQUIRED)
# <X11/Xlib.h> is still included by extUtil.h; the include dir is normally the
# default /usr/include, but non-standard prefixes are picked up here.
find_package(X11)

# --- cgx core C sources (mirrors Makefile.inc SLIB) ---
# Step 6 exclusions (both files 100% dead code — verified zero callers):
#   XFunktions.c  (calcOffset/getColormap/storeColors, X11/GLUT colormap path)
#   readStdCmap.c (readStdCmap, XOpenDisplay-based colormap probe)
set(CGX_SLIB
  AsplitA.c
  AsplitL.c
  adjustMidsideNode.c
  compareStrings.c
  badelems.c
  bodyMesh2.c
  bsort.c
  bsortf.c
  bsorti.c
  calcNormalen.c
  calcPrinc.c
  calcPvector.c
  calcWeight.c
  checkIfNumber.c
  cgx.c
  compare.c
  copyEntity.c
  corrad.c
  dataGeo.c
  dataMesh.c
  defineEntity.c
  dispLists.c
  elemChecker.c
  extFunktions.c
  extGL.c
  fillBody2.c
  frecord.c
  foamFaces.c
  gl3grades.c
  getGeoDataTria.c
  graph.c
  ifind.c
  iinsert.c
  improveBadTr3.c
  improveMesh.c
  iniMeshData.c
  intpol.c
  intpol2.c
  intpol3.c
  iremove.c
  linelength.c
  m_copy.c
  m_sub.c
  mergEntity.c
  mesh2d.c
  meshSet.c
  meshSurf.c
  messages.c
  near3d.c
  normdist.c
  nurbl2seq.c
  orient.c
  p_angle.c
  parser.c
  pickFunktions.c
  plotFunktions.c
  readAnsysList.c
  readDynaList.c
  readEdges.c
  readfrd.c
  readFoam.c
  readStl.c
  readVtk.c
  readccx.c
  readDuns.c
  readNastran.c
  readNG.c
  readTG.c
  readIsaac.c
  readstep.c
  readWf.c
  rectcyl.c
  renumberfrd.c
  selectDisplayFaces.c
  sendMpc.c
  sendSet.c
  setFunktions.c
  shapeFunctions.c
  spline.c
  splitElementsToTets.c
  stof.c
  stoi.c
  stopClock.c
  stos.c
  strfind.c
  strsplt.c
  sins.c
  surfMesh2.c
  sword.c
  trackball.c
  v_distA.c
  v_rec2cyl.c
  v_add.c
  v_angle.c
  v_angle_ref.c
  v_betrag.c
  v_matmult.c
  v_norm.c
  v_prod.c
  v_result.c
  v_scal.c
  v_rot.c
  v_sprod.c
  m_prod.c
  m_prodtr.c
  write2aba.c
  write2ansys.c
  write2aster.c
  write2darwin.c
  write2dolfyn.c
  write2dyna.c
  write2isaac.c
  write2duns.c
  write2frd.c
  write2foam.c
  write2nas.c
  write2samcef.c
  write2tochnog.c
  writefbd.c
  writebp.c
  contact.c
  makeTriFromElems.c
  userFunction.c
)

# Step 6: vendored GLUT 3.5 sources REMOVED from the build. Every remaining
# glut* call site is served by Qt shims (windows, display, input, menus,
# bitmap text) or parked as no-op (init/reshape/visibility).
# Step 14: the last external glut-3.5 dependency (the <GL/glut_cgx.h>
# constants header) is gone too — src/glut_constants.h defines the GLUT_*
# values cgx uses, so no GLUT include dir and no glut-3.5/ sibling are
# needed any more (libSNL stays: it is compiled and linked).
# (Full SUTIL list preserved in Makefile.legacy.bak for reference.)

# --- C++ sources (Makefile compiled+linked these in one g++ step; here proper objects) ---
file(GLOB LIBSNL_SRCS CONFIGURE_DEPENDS "${CGX_LIBSNL_DIR}/*.cpp")
list(LENGTH LIBSNL_SRCS _cgx_libsnl_count)
if(_cgx_libsnl_count EQUAL 0)
  message(FATAL_ERROR
    "No .cpp files found in '${CGX_LIBSNL_DIR}' — libSNL is missing or empty. "
    "Restore it or pass -DCGX_LIBSNL_DIR=<path-to-libSNL/src>.")
endif()
set(CGX_CXX_SRCS
  uselibSNL.cpp
  generateTet.cpp
  ${LIBSNL_SRCS}
)

# --- Qt application shell (Steps 2-3): real main(), main window, GL views,
# --- GLUT window/display/loop shims. CGX_QT activates qt_shim.h in cgx.h. ---
set(CGX_QT_SRCS
  qt/main.cpp
  qt/glue.cpp
  qt/CgxMainWindow.cpp
  qt/CgxViews.cpp
  qt/CgxFont.cpp
  qt/ConsoleCapture.cpp
  qt/ConsoleView.cpp
  qt/Hardcopy.cpp
  qt/DisplaySets.cpp
  qt/DisplaySetsBridge.c
  qt/selftest.cpp
)

add_executable(cgx ${CGX_SLIB} ${CGX_CXX_SRCS} ${CGX_QT_SRCS})

# Rename original main() in cgx.c to cgx_main() so qt/main.cpp owns entry.
set_source_files_properties(cgx.c PROPERTIES COMPILE_DEFINITIONS "main=cgx_main")

target_compile_definitions(cgx PRIVATE SEMINIT CGX_QT)

# Test hooks (qt/selftest.cpp): the in-app selftest and the scripted startup
# keys (CGX_QT_SELFTEST / CGX_QT_SELFTEST_CMDS / CGX_QT_SELFTEST_WHEEL /
# CGX_QT_KEYS / CGX_QT_SHOT_DIR) are compiled in ONLY on request:
#   cmake -B build-test -S . -DCGX_QT_TESTHOOKS=ON
# A default build reads none of these environment variables.
option(CGX_QT_TESTHOOKS "Compile the environment-driven selftest/scripting hooks" OFF)
if(CGX_QT_TESTHOOKS)
  target_compile_definitions(cgx PRIVATE CGX_QT_TESTHOOKS)
endif()
target_compile_options(cgx PRIVATE
  -O2 -Wall -Wno-narrowing
  $<$<COMPILE_LANGUAGE:C>:-std=gnu17>
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
)
# System include dirs (/usr/include, /usr/include/GL) are implicit for the
# compiler; only the project dirs and a non-standard X11 prefix are added.
target_include_directories(cgx PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CGX_LIBSNL_DIR}
)
if(X11_FOUND AND NOT X11_INCLUDE_DIR STREQUAL "/usr/include")
  target_include_directories(cgx PRIVATE ${X11_INCLUDE_DIR})
endif()

# Step 6: X11/Xt/Xmu/Xext/Xi/SM/ICE dropped — the only Xlib calls lived in
# the excluded XFunktions.c/readStdCmap.c (dead code). Remaining X11 types
# (Display/Colormap/XColor globals) are compile-time only; libx11-dev stays
# a build dependency for the headers. GL/GLU come from FindOpenGL so no
# library directory is hardcoded (works for any prefix and any architecture).
target_link_libraries(cgx PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
  OpenGL::GL OpenGL::GLU
  Threads::Threads
  m
)
# librt exists on glibc systems only; link it when the platform has it.
find_library(CGX_RT_LIBRARY rt)
if(CGX_RT_LIBRARY)
  target_link_libraries(cgx PRIVATE ${CGX_RT_LIBRARY})
endif()

# --- build / install gates (cmake/RunCgxCheck.cmake) -----------------------
# The check proves two things about the binary that just got built/installed:
#   * `cgx -bg <example>` runs to ready/done (headless, always), and
#   * `cgx --glcheck` can create a desktop OpenGL context — the regression
#     that otherwise leaves Wayland with NO window at all.
# Parts that cannot run in the current environment are skipped, and every
# child call has a hard timeout, so neither the build nor `cmake --install`
# can hang. Disable with -DCGX_BUILD_CHECK=OFF / -DCGX_INSTALL_CHECK=OFF.
option(CGX_BUILD_CHECK "Run the cgx check after every build of the target" ON)
option(CGX_INSTALL_CHECK "Run the cgx check during 'cmake --install'" ON)
set(CGX_CHECK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunCgxCheck.cmake")

if(CGX_BUILD_CHECK)
  add_custom_command(TARGET cgx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DCGX_BIN=$<TARGET_FILE:cgx>
            -DCGX_EXAMPLE=${CGX_CHECK_EXAMPLE}
            -P ${CGX_CHECK_SCRIPT}
    COMMENT "cgx check: headless smoke + GL/window probe"
    VERBATIM)
endif()

enable_testing()
add_test(NAME cgx_check
  COMMAND ${CMAKE_COMMAND} -DCGX_BIN=$<TARGET_FILE:cgx>
          -DCGX_EXAMPLE=${CGX_CHECK_EXAMPLE}
          -P ${CGX_CHECK_SCRIPT})

add_custom_target(check
  COMMAND ${CMAKE_COMMAND} -DCGX_BIN=$<TARGET_FILE:cgx>
          -DCGX_EXAMPLE=${CGX_CHECK_EXAMPLE}
          -P ${CGX_CHECK_SCRIPT}
  DEPENDS cgx
  COMMENT "cgx check: headless smoke + GL/window probe"
  VERBATIM)

# --- install (prefix is free: -DCMAKE_INSTALL_PREFIX=... or --prefix=...) ---
include(GNUInstallDirs)
install(TARGETS cgx RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(CGX_INSTALL_CHECK)
  # The same check, run against the freshly installed binary, so a broken GL
  # setup fails the install instead of shipping a cgx that opens no window.
  # ${...} below expands now, \${...} expands at install time.
  install(CODE "
    set(_cgx_prefix \"\${CMAKE_INSTALL_PREFIX}\")
    set(_cgx_destdir \"\$ENV{DESTDIR}\")
    if(NOT _cgx_destdir STREQUAL \"\")
      set(_cgx_bin \"\${_cgx_destdir}\${_cgx_prefix}/${CMAKE_INSTALL_BINDIR}/cgx\")
    else()
      set(_cgx_bin \"\${_cgx_prefix}/${CMAKE_INSTALL_BINDIR}/cgx\")
    endif()
    if(\"${CMAKE_CROSSCOMPILING}\")
      message(STATUS \"cgx install check skipped (cross-compiling)\")
    else()
      message(STATUS \"cgx install check: \${_cgx_bin}\")
      execute_process(
        COMMAND \"${CMAKE_COMMAND}\"
                -DCGX_BIN=\${_cgx_bin}
                -DCGX_EXAMPLE=${CGX_CHECK_EXAMPLE}
                -P \"${CGX_CHECK_SCRIPT}\"
        RESULT_VARIABLE _cgx_check_rc)
      if(NOT \"\${_cgx_check_rc}\" STREQUAL \"0\")
        message(FATAL_ERROR
          \"cgx install check failed (rc=\${_cgx_check_rc}) - refusing to install a \"
          \"binary that fails the headless smoke or the OpenGL/window check (a GL \"
          \"setup like this opens NO window on Wayland). Re-run with \"
          \"-DCGX_INSTALL_CHECK=OFF to install anyway.\")
      endif()
    endif()
  ")
endif()

