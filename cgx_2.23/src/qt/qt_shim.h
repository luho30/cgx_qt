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

#pragma once
// Step 3: redirect legacy GLUT window/display/state/loop calls to the Qt
// shims in qt/glue.h. Everything else (menus, bitmap fonts, colors,
// glutInit*) keeps using real vendored GLUT until Steps 4-6.
//
// Included at the end of cgx.h under CGX_QT so all C sources that include
// cgx.h (directly or indirectly) get the same mapping. Vendored GLUT
// sources include only glutint.h and are unaffected.
#include "qt/glue.h"

/* window management -> Qt widgets */
#define glutCreateWindow cgxCreateWindow
#define glutCreateSubWindow cgxCreateSubWindow
#define glutDestroyWindow cgxDestroyWindow
#define glutSetWindow cgxSetWindow
#define glutGetWindow cgxGetWindow

/* display / buffer control */
#define glutPostRedisplay cgxPostRedisplay
#define glutPostWindowRedisplay cgxPostWindowRedisplay
#define glutSwapBuffers cgxSwapBuffers
#define glutDisplayFunc cgxDisplayFunc
/* overlay clear: ensure HUD overlays (w0 legend, w2 axes) clear to 100% transparent */
#define glClearColor cgxClearColor

/* state queries and geometry */
#define glutGet cgxGet
#define glutReshapeWindow cgxReshapeWindow
#define glutPositionWindow cgxPositionWindow
#define glutSetWindowTitle cgxSetWindowTitle
#define glutSetIconTitle cgxSetIconTitle

/* event loop */
#define glutIdleFunc cgxIdleFunc
#define glutMainLoop cgxMainLoop
#define glutAttachMenu cgxAttachMenu
#define glutDetachMenu cgxDetachMenu

/* Step 5: popup menus fully mirrored (see glue.h) */
#define glutCreateMenu cgxCreateMenu
#define glutDestroyMenu cgxDestroyMenu
#define glutGetMenu cgxGetMenu
#define glutSetMenu cgxSetMenu
#define glutAddMenuEntry cgxAddMenuEntry
#define glutAddSubMenu cgxAddSubMenu
#define glutRemoveMenuItem cgxRemoveMenuItem
#define glutChangeToMenuEntry cgxChangeToMenuEntry
#define glutChangeToSubMenu cgxChangeToSubMenu

/* Step 6: bitmap text via QFont (CgxFont.cpp); vendored GLUT sources are no
   longer compiled, so their font globals are gone. glutInit* only opened
   the X display for GLUT's benefit — Qt owns all contexts now. */
#define glutBitmapCharacter cgxBitmapCharacter
#define glutBitmapWidth cgxBitmapWidth
/* NOTE: only symbols with real cgx* implementations may be mapped here.
   Any future upstream glut call NOT listed will fail to link (by design) so
   the replay in QT-PORT.md §5 notices it instead of silently no-op'ing. */
#define glutInit(a, b) ((void)0)
#define glutInitDisplayMode(m) ((void)0)
#define glutInitWindowPosition(x, y) ((void)0)
#define glutInitWindowSize(w, h) ((void)0)

/* Step 4: input callbacks stored per window (see glue.h); Qt views
   forward press/release/motion/keys to them */
#define glutMouseFunc cgxMouseFunc
#define glutKeyboardFunc cgxKeyboardFunc
#define glutSpecialFunc cgxSpecialFunc
#define glutMotionFunc cgxMotionFunc
#define glutPassiveMotionFunc cgxPassiveMotionFunc

/* reshape driven by CgxMainWindow::resizeEvent; entry/visibility have no
   Qt equivalent wired yet (parked) */
#define glutReshapeFunc(f) ((void)0)
#define glutEntryFunc(f) ((void)0)
#define glutVisibilityFunc(f) ((void)0)
