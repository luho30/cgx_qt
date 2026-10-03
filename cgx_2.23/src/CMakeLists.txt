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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

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
file(GLOB LIBSNL_SRCS ../../libSNL/src/*.cpp)
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
  qt/selftest.cpp
)

add_executable(cgx ${CGX_SLIB} ${CGX_CXX_SRCS} ${CGX_QT_SRCS})

# Rename original main() in cgx.c to cgx_main() so qt/main.cpp owns entry.
set_source_files_properties(cgx.c PROPERTIES COMPILE_DEFINITIONS "main=cgx_main")

target_compile_definitions(cgx PRIVATE SEMINIT CGX_QT)
target_compile_options(cgx PRIVATE
  -O2 -Wall -Wno-narrowing
  $<$<COMPILE_LANGUAGE:C>:-std=gnu17>
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
)
target_include_directories(cgx PRIVATE
  ./
  /usr/include
  /usr/include/GL
  ../../libSNL/src
)
# Legacy /usr/X11/include rarely exists on modern Ubuntu; keep if present.
if(EXISTS "/usr/X11/include")
  target_include_directories(cgx PRIVATE /usr/X11/include)
endif()

target_link_directories(cgx PRIVATE /usr/lib/x86_64-linux-gnu)
# Step 6: X11/Xt/Xmu/Xext/Xi/SM/ICE dropped — the only Xlib calls lived in
# the excluded XFunktions.c/readStdCmap.c (dead code). Remaining X11 types
# (Display/Colormap/XColor globals) are compile-time only; libx11-dev stays
# a build dependency for the headers.
target_link_libraries(cgx PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
  GL GLU m pthread rt
)
