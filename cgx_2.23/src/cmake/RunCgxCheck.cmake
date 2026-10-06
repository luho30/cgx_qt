# RunCgxCheck.cmake — build/install gate for the cgx binary.
#
#   cmake -DCGX_BIN=<path to cgx> [-DCGX_EXAMPLE=<fbd>] [-DCGX_REQUIRE_GL=ON] \
#         -P RunCgxCheck.cmake
#
# 1. headless functional smoke: `<bin> -bg <example>` must exit 0 and report
#    ready/done. Needs no display, so it always runs.
# 2. GL/window probe: `<bin> --glcheck` must exit 0 (context creatable, desktop
#    OpenGL, not GLES — i.e. the regression that leaves Wayland with NO window).
#    Needs a display: skipped when neither DISPLAY nor WAYLAND_DISPLAY is set,
#    unless CGX_REQUIRE_GL=ON forces it (then a missing display is an error).
#
# Every child process has a hard TIMEOUT, so this can never hang a build or a
# `cmake --install`. Any failure is FATAL_ERROR -> non-zero exit.

if(NOT DEFINED CGX_BIN OR CGX_BIN STREQUAL "")
  message(FATAL_ERROR "cgx check: -DCGX_BIN=<path> is required")
endif()
if(NOT EXISTS "${CGX_BIN}")
  message(FATAL_ERROR "cgx check: binary not found: ${CGX_BIN}")
endif()

# --- 1. headless smoke (always) ------------------------------------------
if(DEFINED CGX_EXAMPLE AND NOT CGX_EXAMPLE STREQUAL "" AND EXISTS "${CGX_EXAMPLE}")
  execute_process(
    COMMAND "${CGX_BIN}" -bg "${CGX_EXAMPLE}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 120)
  if(NOT "${_rc}" STREQUAL "0")
    message(FATAL_ERROR "cgx check: headless run failed (rc=${_rc})\n${_out}${_err}")
  endif()
  if(NOT _out MATCHES "ready" OR NOT _out MATCHES "done")
    message(FATAL_ERROR
      "cgx check: headless run of ${CGX_EXAMPLE} never reported ready/done\n${_out}${_err}")
  endif()
  message(STATUS "cgx check: headless smoke OK (${CGX_EXAMPLE})")
else()
  message(STATUS "cgx check: example '${CGX_EXAMPLE}' not present - headless smoke skipped")
endif()

# --- 2. GL/window probe ----------------------------------------------------
set(_have_display FALSE)
if(NOT "$ENV{DISPLAY}" STREQUAL "" OR NOT "$ENV{WAYLAND_DISPLAY}" STREQUAL "")
  set(_have_display TRUE)
endif()

if(_have_display OR CGX_REQUIRE_GL)
  execute_process(
    COMMAND "${CGX_BIN}" --glcheck
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 60)
  if(NOT "${_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "cgx check: GL probe FAILED (rc=${_rc}) - a binary built like this opens NO window "
      "on Wayland (EGL_BAD_MATCH / QOpenGLWidget gets no context).\n"
      "stdout:\n${_out}\nstderr:\n${_err}")
  endif()
  string(STRIP "${_out}" _out)
  message(STATUS "cgx check: ${_out}")
else()
  message(STATUS
    "cgx check: no display (DISPLAY and WAYLAND_DISPLAY unset) - GL/window probe "
    "skipped; headless smoke passed")
endif()

message(STATUS "cgx check: all checks passed for ${CGX_BIN}")
