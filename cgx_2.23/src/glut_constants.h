/* --------------------------------------------------------------------  */
/*                          CALCULIX                                     */
/*                   - GRAPHICAL INTERFACE -                             */
/*                                                                       */
/*     A 3-dimensional pre- and post-processor for finite elements       */
/*              Copyright (C) 1996 Klaus Wittig                          */
/*                                                                       */
/*     This program is free software; you can redistribute it and/or     */
/*     modify it under the terms of the GNU General Public License as    */
/*     published by the Free Software Foundation; version 2 of           */
/*     the License.                                                      */
/*                                                                       */
/*     This program is distributed in the hope that it will be useful,   */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of    */
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      */
/*     GNU General Public License for more details.                      */
/*                                                                       */
/*     You should have received a copy of the GNU General Public License */
/*     along with this program; if not, write to the Free Software       */
/*     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.         */
/* --------------------------------------------------------------------  */

/* GLUT constants used by cgx (QT-PORT.md, Step 14).
 *
 * Replaces the vendored <GL/glut_cgx.h>: cgx only ever needed these
 * *values* — no GLUT code is compiled or linked, every glut*() call is
 * served by the Qt shims in qt/glue.cpp (declared in qt/qt_shim.h).
 * The values are the standard GLUT 3.7 ones, kept bit-identical so no
 * numeric behaviour can drift:
 *   - button/state codes reach cgx.c through the shims (qtToGlutButton),
 *   - the window/screen/state query codes are what glutGet() answers,
 *   - under CGX_QT, cgx.h redefines GLUT_FONT to the positional tokens
 *     (void*)0..5 (same order), which CgxFont.cpp maps to QFonts — the
 *     GLUT_BITMAP_* names below keep GLUT's own (void*) values for the
 *     original GLUT_FONT macro at the top of cgx.h.
 * The wheel codes and the menu/font geometry macros are cgx-local and
 * stay in cgx.c / cgx.h (GLUT_WEEL_UP/DOWN, GLUT_FONT*, GLUT_MENU_*).
 */

#ifndef CGX_GLUT_CONSTANTS_H
#define CGX_GLUT_CONSTANTS_H

/* mouse buttons and button states */
#define GLUT_LEFT_BUTTON   0
#define GLUT_MIDDLE_BUTTON 1
#define GLUT_RIGHT_BUTTON  2
#define GLUT_DOWN          0
#define GLUT_UP            1

/* glutGet() query codes — window, screen, timing */
#define GLUT_WINDOW_X           100
#define GLUT_WINDOW_Y           101
#define GLUT_WINDOW_WIDTH       102
#define GLUT_WINDOW_HEIGHT      103
#define GLUT_SCREEN_WIDTH       200
#define GLUT_SCREEN_HEIGHT      201
#define GLUT_ELAPSED_TIME       700
#define GLUT_INIT_WINDOW_X      500
#define GLUT_INIT_WINDOW_Y      501
#define GLUT_INIT_WINDOW_WIDTH  502
#define GLUT_INIT_WINDOW_HEIGHT 503

/* glutGet() query codes — display mode and menu state */
#define GLUT_RGB           0
#define GLUT_RGBA          GLUT_RGB
#define GLUT_DOUBLE        2
#define GLUT_DEPTH         16
#define GLUT_NOT_VISIBLE   0
#define GLUT_ENTERED       1
#define GLUT_MENU_NUM_ITEMS 300

/* special key codes delivered to the keyboard callback */
#define GLUT_KEY_F1       1
#define GLUT_KEY_UP       101
#define GLUT_KEY_DOWN     103
#define GLUT_KEY_LEFT     100
#define GLUT_KEY_RIGHT    102
#define GLUT_KEY_PAGE_UP  104
#define GLUT_KEY_PAGE_DOWN 105

/* bitmap font names (GLUT's own (void*) values; see header comment) */
#define GLUT_BITMAP_9_BY_15        ((void*)2)
#define GLUT_BITMAP_8_BY_13        ((void*)3)
#define GLUT_BITMAP_TIMES_ROMAN_10 ((void*)4)
#define GLUT_BITMAP_TIMES_ROMAN_24 ((void*)5)
#define GLUT_BITMAP_HELVETICA_12   ((void*)7)
#define GLUT_BITMAP_HELVETICA_18   ((void*)8)

#endif /* CGX_GLUT_CONSTANTS_H */
