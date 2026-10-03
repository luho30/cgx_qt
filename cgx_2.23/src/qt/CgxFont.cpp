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

// Step 6: Qt-backed replacement for the vendored GLUT bitmap fonts.
//
// Legacy tokens (void*)0..5 select the six GLUT fonts in cgx.h order:
//   0 TIMES_ROMAN_10, 1 HELVETICA_12, 2 8x13, 3 9x15, 4 HELVETICA_18,
//   5 TIMES_ROMAN_24. Each maps to a QFont with matching pixel size.
// Glyphs are rasterized once into cached 1-bit bitmaps and drawn with
// glBitmap, advancing the raster position by the real advance — exactly
// the glutBitmapCharacter/glutBitmapWidth contract, so measure and draw
// stay consistent for every caller.
#include "glue.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

#include <GL/gl.h>

#include <cstdint>
#include <map>
#include <vector>

namespace
{
struct Glyph
{
  int width = 0; // bitmap width in pixels
  int height = 0; // bitmap height in pixels
  int xorig = 0; // glBitmap origin offsets
  int yorig = 0;
  int advance = 0; // raster advance (== cgxBitmapWidth)
  std::vector<GLubyte> bits; // 1-bit MSB-first, GL bottom row first
};

// QFont pixel sizes chosen to match the legacy GLUT glyph heights.
int cgxFontPixelSize(int token)
{
  static const int sizes[6] = {10, 12, 13, 15, 18, 24};
  return (token >= 0 && token < 6) ? sizes[token] : 13;
}

const QFont &cgxQFont(int token)
{
  static QFont fonts[6];
  static bool initialized = false;
  if (!initialized)
  {
    fonts[0] = QFont(QStringLiteral("Times New Roman"));
    fonts[0].setStyleHint(QFont::Serif);
    fonts[1] = QFont(QStringLiteral("Helvetica"));
    fonts[1].setStyleHint(QFont::SansSerif);
    fonts[2] = QFont(QStringLiteral("Monospace"));
    fonts[2].setStyleHint(QFont::TypeWriter);
    fonts[3] = QFont(QStringLiteral("Monospace"));
    fonts[3].setStyleHint(QFont::TypeWriter);
    fonts[4] = QFont(QStringLiteral("Helvetica"));
    fonts[4].setStyleHint(QFont::SansSerif);
    fonts[5] = QFont(QStringLiteral("Times New Roman"));
    fonts[5].setStyleHint(QFont::Serif);
    for (int i = 0; i < 6; ++i)
    {
      fonts[i].setPixelSize(cgxFontPixelSize(i));
      fonts[i].setStyleStrategy(QFont::NoAntialias);
    }
    initialized = true;
  }
  if (token < 0 || token >= 6)
    token = 2;
  return fonts[token];
}

int cgxFontToken(void *font)
{
  // Tokens are (void*)0..5 (see GLUT_FONT under CGX_QT in cgx.h).
  const intptr_t v = reinterpret_cast<intptr_t>(font);
  if (v >= 0 && v < 6)
    return (int)v;
  return 2; // unknown handle -> fixed default font
}

const Glyph &cgxGlyph(int token, unsigned int ch)
{
  static std::map<uint64_t, Glyph> cache;
  const uint64_t key = ((uint64_t)(uint32_t)token << 32) | ch;
  auto it = cache.find(key);
  if (it != cache.end())
    return it->second;

  Glyph g;
  const QFont &font = cgxQFont(token);
  const QFontMetrics fm(font);
  const QChar qch(ch);
  g.advance = fm.horizontalAdvance(qch);
  const int ascent = fm.ascent();
  const int descent = fm.descent();
  const int cellH = (ascent + descent > 0) ? (ascent + descent) : 1;
  // Cell wide enough for the ink plus 1px pads (italic overhang etc.).
  const int cellW = ((g.advance > 0) ? g.advance : 1) + 2;

  QImage img(cellW, cellH, QImage::Format_Grayscale8);
  img.fill(0);
  {
    QPainter p(&img);
    p.setFont(font);
    p.setPen(Qt::white);
    // Baseline at y=ascent, 1px left pad; bearings shift ink naturally.
    p.drawText(1, ascent, QString(qch));
  }
  // Pack 1-bit MSB-first, GL bottom row first (QImage is top row first).
  const int rowBytes = (cellW + 7) / 8;
  g.width = cellW;
  g.height = cellH;
  g.xorig = 1;
  g.yorig = cellH - 1 - ascent; // baseline lands on the raster position
  g.bits.assign(rowBytes * cellH, 0);
  for (int r = 0; r < cellH; ++r)
  {
    const int srcRow = cellH - 1 - r;
    const uchar *scan = img.constScanLine(srcRow);
    for (int c = 0; c < cellW; ++c)
    {
      if (scan[c] > 127)
        g.bits[r * rowBytes + c / 8] |= (GLubyte)(0x80 >> (c % 8));
    }
  }
  return cache.emplace(key, std::move(g)).first->second;
}
} // namespace

void cgxBitmapCharacter(void *font, int character)
{
  if (!qApp)
    return; // no GUI context (e.g. -bg never draws text anyway)
  const int token = cgxFontToken(font);
  const Glyph &g = cgxGlyph(token, (unsigned int)(unsigned char)character);
  // Rows are (cellW+7)/8 bytes — almost never 4-byte aligned, while the GL
  // default unpack alignment is 4. Without this, every glyph row shears.
  GLint prevAlignment = 4;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glBitmap(g.width, g.height, (GLfloat)g.xorig, (GLfloat)g.yorig, (GLfloat)g.advance, 0.0f,
           g.bits.data());
  glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlignment);
}

int cgxBitmapWidth(void *font, int character)
{
  if (!qApp)
    return 8;
  const int token = cgxFontToken(font);
  return cgxGlyph(token, (unsigned int)(unsigned char)character).advance;
}
