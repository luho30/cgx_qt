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
// Step 11: capture everything cgx prints to stdout so it can be shown in the
// in-window console panel, WITHOUT touching the legacy C code (all ~10,500
// output calls are plain printf to stdout; stderr is never used).
//
// Mechanism (GUI mode only, never for -bg):
//   - the original stdout fd is saved with dup();
//   - a pipe replaces fd 1 (dup2), stdout becomes line-buffered;
//   - a dedicated reader thread drains the pipe, writes every byte to the
//     saved original fd (so terminal / `> log` output is unchanged) and
//     appends it to a mutex-protected buffer;
//   - the GUI thread polls takeNew().
// The reader thread is essential: cgx prints from the GUI thread, and if that
// same thread also had to drain the pipe, the 64 KB pipe would fill during a
// big read (thousands of lines) and cgx would deadlock.
#include <QString>

namespace cgxconsole
{
// Idempotent. Call before cgx_main() so the banner is captured too.
void install();

// Returns the text captured since the last call (control characters other
// than \n \r \b \t are removed; invalid UTF-8 is replaced).
QString takeNew();

// Total bytes captured / dropped (buffer overflow protection), for tests.
unsigned long long bytesCaptured();
unsigned long long bytesDropped();
} // namespace cgxconsole
