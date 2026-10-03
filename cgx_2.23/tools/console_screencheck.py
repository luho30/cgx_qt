#!/usr/bin/env python3
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

"""Real-screen check that the Step 11 console panel is stacked ABOVE the legend.

Why external: QWidget::grab() / QScreen::grabWindow() do not reproduce the
on-screen compositing of the stacked-on-top QOpenGLWidgets (grab() ordering
differs; grabWindow() returns black), so this is the only reliable check.

Needs an X11 session (runs cgx with QT_QPA_PLATFORM=xcb) and PIL.
usage (from src/):  python3 ../tools/console_screencheck.py [model.frd]
exit 0 = pass, 1 = fail.  Prints the numbers it based the decision on.
"""
import os, re, subprocess, sys, time
from PIL import ImageGrab

model = sys.argv[1] if len(sys.argv) > 1 else '../examples/result.frd'
env = os.environ.copy()
env['QT_QPA_PLATFORM'] = 'xcb'
# result + dataset (shows the legend colour bar), zoom in so the model is under
# the legend/panel, switch the command line (and with it the console) on.
env['CGX_QT_KEYS'] = 'ds 1 e 1;view cl;help'
p = subprocess.Popen(['./build/cgx', model], env=env, stdout=subprocess.DEVNULL,
                     stderr=subprocess.PIPE, text=True)
geom = None
t0 = time.time()
while time.time() - t0 < 60:
    line = p.stderr.readline()
    if not line:
        break
    m = re.search(r'KEYS-DONE view=(\d+),(\d+),(\d+),(\d+) console=(\d+),(\d+),(\d+),(\d+) consoleVisible=(\d)', line)
    if m:
        geom = list(map(int, m.groups()))
        break
if not geom:
    p.kill(); print('FAIL: app did not report geometry'); sys.exit(1)
time.sleep(1.5)
img = ImageGrab.grab().convert('RGB')
p.terminate()
try: p.wait(timeout=5)
except Exception: p.kill()
vx, vy, vw, vh, cx, cy, cw, ch, vis = geom
if not vis:
    print('FAIL: console not visible'); sys.exit(1)
px = img.load()
def sat(x, y):
    r, g, b = px[x, y]; return max(r, g, b) - min(r, g, b)
# legend colour bar sits in the first ~14 px of the 3D view's left edge
outside = max(sat(vx + x, y) for x in range(1, 14) for y in range(vy, cy))
inside = max(sat(vx + x, y) for x in range(1, 14) for y in range(cy + 2, cy + ch - 2))
print(f'colour-bar saturation above panel={outside} inside panel={inside} '
      f'(panel {cw}x{ch} at {cx},{cy})')
if outside < 180:
    print('FAIL: no colour bar found above the panel (test not meaningful)'); sys.exit(1)
if inside > 150:
    print('FAIL: console is NOT above the legend'); sys.exit(1)
print('PASS: console is above the legend, which is tinted by the 70% panel')
