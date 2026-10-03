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

// Step 12: Qt-native hardcopy. Replaces the legacy ImageMagick pipeline
// (mogrify/composite/convert via system(), with unconditional
// while(access()) waits that stall forever when the tools are missing —
// which they are on a stock Ubuntu: no convert/mogrify/composite at all).
//
// Interception is source-level (cgx.h maps the three names here under
// CGX_QT and compiles the legacy bodies out, so every call site — including
// constant-argument ones the linker --wrap approach could not catch, see
// QT-PORT.md — lands here).
//
// Design:
//   - capture: grabFramebuffer() on each view (the sanctioned API — never
//     raw glReadPixels with legacy-sized mallocs). The 3D view is opaque and
//     full-window; the legend and axes grabs (with alpha) are painted over it
//     at their real widget geometry. No stale width_menu offsets.
//   - everything runs deferred (queued singleshot): createHardcopy is also
//     called from inside DrawGrafic* paint paths (movie frames), where
//     grabbing would re-enter painting. The wrapper only enqueues; the slot
//     captures after the paint finished. FIFO preserves order.
//   - formats: PNG via QImage, TGA via a small writer below, PS as
//     raster-EPS via a small writer below (the legacy PS was raster too),
//     GIF frames + movie assembly via ffmpeg (present on this system) with a
//     keep-PNG-frames fallback and a clear message when it is absent.
//   - counters, file names, console messages and write2stack reporting mirror
//     the legacy createHardcopy exactly.
#include "CgxMainWindow.h"
#include "CgxViews.h"
#include "glue.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

// Legacy state owned by cgx.c / setFunktions.c (all non-static there).
extern "C" {
extern char inpformat;
extern int psNr, tgaNr, gifNr, pngNr;
extern int movieFrames, movieFlag, animList, stopFlag;
extern char movieCommandFile[];
extern char **parameter;
int write2stack(int n, char **parameter);
int pre_read(char *record);
} // extern "C"

// Window ids (plain C++ globals, same as in glue.cpp).
extern int w2;
extern int activWindow;

namespace
{
struct HcpyJob
{
  int selection = 0;
  std::string file;
  bool hasFile = false;
  int movieGen = 0; // recording generation (movie frames only, see below)
};
std::deque<HcpyJob> s_jobs;
bool s_scheduled = false;
// A repaint storm (e.g. a drag) can enqueue more frame jobs than the
// recording asked for; they are still queued when the completion assembly
// already stopped the recording. Each recording gets a generation: jobs from
// an older one are stale backlog and are dropped instead of becoming frames
// of nothing (legacy could not have this race — it ran synchronously).
int s_movieGen = 0;
QString s_ffmpeg; // resolved once
bool s_ffmpegProbed = false;

bool haveFfmpeg()
{
  if (!s_ffmpegProbed)
  {
    s_ffmpegProbed = true;
    s_ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  }
  return !s_ffmpeg.isEmpty();
}

// Uncompressed 24-bit TGA, bottom-up rows (correct orientation; the legacy
// writer stored top-down without the origin bit).
bool writeTGA(const QString &path, const QImage &img)
{
  const QImage rgb = img.convertToFormat(QImage::Format_RGB888);
  FILE *f = std::fopen(path.toLocal8Bit().constData(), "wb");
  if (!f)
    return false;
  const int w = rgb.width(), h = rgb.height();
  unsigned char hdr[18] = {};
  hdr[2] = 2;
  hdr[12] = (unsigned char)(w & 0xff);
  hdr[13] = (unsigned char)((w >> 8) & 0xff);
  hdr[14] = (unsigned char)(h & 0xff);
  hdr[15] = (unsigned char)((h >> 8) & 0xff);
  hdr[16] = 24;
  bool ok = std::fwrite(hdr, 1, 18, f) == 18;
  for (int y = h - 1; ok && y >= 0; --y)
  {
    const unsigned char *scan = rgb.constScanLine(y);
    for (int x = 0; x < w; ++x)
    {
      const unsigned char bgr[3] = {scan[3 * x + 2], scan[3 * x + 1], scan[3 * x]};
      if (std::fwrite(bgr, 1, 3, f) != 3)
      {
        ok = false;
        break;
      }
    }
  }
  std::fclose(f);
  return ok;
}

// Raster EPS (the legacy .ps files were raster via convert, too). Painted as
// run-length encoded rectfills — deliberately NOT via the image/colorimage
// operators: Ghostscript 10.06 renders only a ~2x2 corner of procedure-fed
// images fromHex-batch files (verified: vector rectfill of the same geometry
// is pixel-exact while colorimage is not), so only setrgbcolor+rectfill are
// used here. Pure-white runs are skipped (white page). PS origin is
// bottom-left, hence the flipped row index.
bool writeEPS(const QString &path, const QImage &img)
{
  const QImage rgb = img.convertToFormat(QImage::Format_RGB888);
  FILE *f = std::fopen(path.toLocal8Bit().constData(), "wb");
  if (!f)
    return false;
  const int w = rgb.width(), h = rgb.height();
  std::fprintf(f, "%%!PS-Adobe-3.0 EPSF-3.0\n%%%%Creator: cgx Qt port (no ImageMagick)\n"
                  "%%%%BoundingBox: 0 0 %d %d\n%%%%EndComments\n",
             w, h);
  char last[32] = "";
  for (int y = 0; y < h; ++y)
  {
    const unsigned char *scan = rgb.constScanLine(y);
    int x = 0;
    while (x < w)
    {
      const int r = scan[3 * x], g = scan[3 * x + 1], b = scan[3 * x + 2];
      int x1 = x + 1;
      while (x1 < w && scan[3 * x1] == r && scan[3 * x1 + 1] == g && scan[3 * x1 + 2] == b)
        ++x1;
      if (r != 255 || g != 255 || b != 255)
      {
        char col[32];
        std::snprintf(col, sizeof(col), "%.3f %.3f %.3f", r / 255.0, g / 255.0, b / 255.0);
        if (std::strcmp(col, last) != 0)
        {
          std::fprintf(f, "%s setrgbcolor\n", col);
          std::strcpy(last, col);
        }
        std::fprintf(f, "%d %d %d 1 rectfill\n", x, h - 1 - y, x1 - x);
      }
      x = x1;
    }
  }
  std::fputs("showpage\n", f);
  std::fclose(f);
  return true;
}

bool gifCapable()
{
  if (haveFfmpeg())
    return true;
  for (const QByteArray &f : QImageWriter::supportedImageFormats())
    if (f == "gif")
      return true;
  return false;
}

// Full-window opaque 3D scene with the transparent legend and axes overlays
// blended at their real geometry. Must run on the GUI thread outside paint.
QImage compositedHardcopy()
{
  CgxMainWindow *m = cgxMainWindow();
  if (!m || !m->graphicsView())
    return QImage();
  GraphicsView *g = m->graphicsView();
  QImage canvas = g->grabFramebuffer().convertToFormat(QImage::Format_ARGB32);
  const double dpr = g->devicePixelRatioF();
  if (dpr <= 0.0)
    return QImage();
  QPainter p(&canvas);
  auto place = [&](QOpenGLWidget *w) {
    if (!w || !w->isVisible())
      return;
    const QRect geo = w->geometry(); // container coords == graphics coords
    const QPoint at((int)(geo.x() * dpr), (int)(geo.y() * dpr));
    p.drawImage(at, w->grabFramebuffer());
  };
  place(m->menuView());
  place(m->axesView());
  p.end();
  // The overlays are blended over the opaque 3D scene, so the result is fully
  // opaque; flatten to keep the files small and alpha-free.
  return canvas.convertToFormat(QImage::Format_RGB888);
}

void removeFrames(const char *pattern)
{
  const QStringList hits =
      QDir(QStringLiteral(".")).entryList(QStringList(QString::fromLatin1(pattern)), QDir::Files);
  for (const QString &f : hits)
    QFile::remove(f);
}

void assembleMovie()
{
  // Mirrors legacy createHardcopy(0) (which drove pre_movie "make"/"clean"):
  // _1.._N.gif -> movie.gif, drop the frames, reset the counter.
  if (gifNr <= 0)
  {
    std::printf("no frames recorded\n");
    return;
  }
  if (!haveFfmpeg())
  {
    std::printf("ffmpeg not found, frames kept as _1.._%d.gif\n", gifNr);
    return;
  }
  // Concat demuxer with an explicit numeric file list (robust to gaps,
  // unlike _%d.gif globbing which also misorders _10 before _2). Note:
  // -framerate is NOT valid here (it belongs to image devices, not the
  // concat demuxer); the output rate is set with -r before the output.
  QTemporaryFile list(QDir::currentPath() + QStringLiteral("/cgx_mov_XXXXXX.txt"));
  if (!list.open())
  {
    std::printf("movie assembly failed, frames kept as _1.._%d.gif\n", gifNr);
    return;
  }
  for (int i = 1; i <= gifNr; ++i)
  {
    const QString f = QStringLiteral("_%1.gif").arg(i);
    if (QFile::exists(f))
      list.write(QStringLiteral("file '%1'\n").arg(f).toUtf8());
  }
  const QString listName = list.fileName();
  list.close(); // keep on disk until ffmpeg is done (autoRemove on destruct)
  std::printf("ffmpeg -f concat -i %s -r 2 movie.gif\n", listName.toUtf8().constData());
  const int rc = QProcess::execute(s_ffmpeg, {QStringLiteral("-y"), QStringLiteral("-loglevel"),
                                              QStringLiteral("error"), QStringLiteral("-f"),
                                              QStringLiteral("concat"), QStringLiteral("-safe"),
                                              QStringLiteral("0"), QStringLiteral("-i"), listName,
                                              QStringLiteral("-r"), QStringLiteral("2"),
                                              QStringLiteral("-loop"), QStringLiteral("0"),
                                              QStringLiteral("movie.gif")});
  if (rc != 0)
    std::printf("movie assembly failed, frames kept as _1.._%d.gif\n", gifNr);
  else
    removeFrames("_*.gif");
  gifNr = 0;
}

// One legacy-equivalent hardcopy. Runs deferred on the GUI thread.
void runHcpyJob(const HcpyJob &job)
{
  char fileName[256];
  const int selection = job.selection;
  if (selection == 0)
  {
    assembleMovie();
    return;
  }
  const QImage comp = compositedHardcopy();
  if (comp.isNull())
  {
    std::printf("hardcopy not possible, no graphics window\n");
    return;
  }
  if (selection == 1)
  {
    psNr++;
    if (job.hasFile)
      std::snprintf(fileName, sizeof(fileName), "%s.ps", job.file.c_str());
    else
      std::snprintf(fileName, sizeof(fileName), "hcpy_%d.ps", psNr);
    std::printf("create %s\n ", fileName);
    if (!writeEPS(QString::fromLocal8Bit(fileName), comp))
      std::printf("ERROR: could not write %s\n", fileName);
    std::sprintf(parameter[0], "%s", fileName);
    std::sprintf(parameter[1], "%d", psNr);
    write2stack(2, parameter);
    std::printf("ready\n");
  }
  else if (selection == 2)
  {
    tgaNr++;
    if (job.hasFile)
    {
      std::snprintf(fileName, sizeof(fileName), "%s.tga", job.file.c_str());
      // Legacy moved hcpy_N.tga onto the name; writing it directly is the
      // same visible result without the temp file.
      std::printf("create %s\n ", fileName);
      if (!writeTGA(QString::fromLocal8Bit(fileName), comp))
        std::printf("ERROR: could not write %s\n", fileName);
    }
    else
    {
      std::snprintf(fileName, sizeof(fileName), "hcpy_%d.tga", tgaNr);
      std::printf("create %s\n ", fileName);
      if (!writeTGA(QString::fromLocal8Bit(fileName), comp))
        std::printf("ERROR: could not write %s\n", fileName);
    }
    std::sprintf(parameter[0], "%s", fileName);
    std::sprintf(parameter[1], "%d", tgaNr);
    write2stack(2, parameter);
    std::printf("ready\n");
  }
  else if (selection == 3)
  {
    if (!movieFlag || job.movieGen != s_movieGen)
      return; // stale backlog from a recording that already completed
    // Movie frame. Legacy converted hcpy_0.tga per frame with convert; here
    // the composited frame goes straight to _N.gif via ffmpeg.
    gifNr++;
    if (!haveFfmpeg())
    {
      std::snprintf(fileName, sizeof(fileName), "_%d.png", gifNr);
      std::printf("ffmpeg not found, keeping frame %s\n", fileName);
      comp.save(QString::fromLocal8Bit(fileName), "PNG");
    }
    else
    {
      QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/cgx_frame_XXXXXX.png"));
      if (tmp.open())
      {
        tmp.close();
        comp.save(tmp.fileName(), "PNG");
        std::snprintf(fileName, sizeof(fileName), "_%d.gif", gifNr);
        std::printf("ffmpeg -i frame.png %s\n", fileName);
        if (QProcess::execute(s_ffmpeg, {QStringLiteral("-y"), QStringLiteral("-loglevel"),
                                          QStringLiteral("error"), QStringLiteral("-i"),
                                          tmp.fileName(),
                                          QString::fromLocal8Bit(fileName)}) != 0)
          std::printf("ERROR: could not write %s\n", fileName);
      }
    }
    if ((movieFrames) && (gifNr >= movieFrames))
    {
      animList = 0;
      movieFrames = 0;
      movieFlag = 0;
      ++s_movieGen; // invalidate any still-queued frames of this recording
      runHcpyJob(HcpyJob{0, {}, false});
      if (movieCommandFile[0])
        pre_read(movieCommandFile);
    }
  }
  else if (selection == 4 || selection == 5)
  {
    const bool isGif = (selection == 4);
    int &nr = isGif ? gifNr : pngNr;
    const char *ext = isGif ? "gif" : "png";
    nr++;
    char base[256];
    if (job.hasFile)
      std::snprintf(base, sizeof(base), "%s", job.file.c_str());
    else
      std::snprintf(base, sizeof(base), "hcpy_%d", nr);
    std::snprintf(fileName, sizeof(fileName), "%s.%s", base, ext);
    std::printf("create %s\n ", fileName);
    bool done = false;
    if (!isGif)
    {
      done = comp.save(QString::fromLocal8Bit(fileName), "PNG");
    }
    else
    {
      for (const QByteArray &f : QImageWriter::supportedImageFormats())
        if (f == "gif")
          done = comp.save(QString::fromLocal8Bit(fileName), "GIF");
      if (!done && haveFfmpeg())
      {
        QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/cgx_frame_XXXXXX.png"));
        if (tmp.open())
        {
          tmp.close();
          comp.save(tmp.fileName(), "PNG");
          std::printf("ffmpeg -i frame.png %s\n", fileName);
          done = QProcess::execute(s_ffmpeg,
                                   {QStringLiteral("-y"), QStringLiteral("-loglevel"),
                                    QStringLiteral("error"), QStringLiteral("-i"), tmp.fileName(),
                                    QString::fromLocal8Bit(fileName)}) == 0;
        }
      }
      if (!done)
      {
        // No GIF writer anywhere: same picture as PNG so nothing is lost.
        std::snprintf(fileName, sizeof(fileName), "%s.png", base);
        std::printf("no GIF writer available, wrote %s instead\n", fileName);
        done = comp.save(QString::fromLocal8Bit(fileName), "PNG");
      }
    }
    if (!done)
      std::printf("ERROR: could not write %s\n", fileName);
    std::sprintf(parameter[0], "%s", fileName);
    std::sprintf(parameter[1], "%d", nr);
    write2stack(2, parameter);
    std::printf("ready\n");
  }
}

void processHcpyJobs()
{
  s_scheduled = false;
  while (!s_jobs.empty())
  {
    const HcpyJob job = s_jobs.front();
    s_jobs.pop_front();
    runHcpyJob(job);
  }
}
} // namespace

void cgxSaveTGAScreenShot(char *filename, int /*w*/, int /*h*/)
{
  // Only reachable if legacy getTGAScreenShot ever ran again (it is wrapped
  // too): capture the currently active target's view. Never called mid-paint
  // by our own code paths.
  CgxMainWindow *m = cgxMainWindow();
  if (!m)
    return;
  QOpenGLWidget *v = m->graphicsView();
  if (activWindow == cgxMenuId())
    v = m->menuView();
  else if (w2 != 0 && activWindow == w2)
    v = m->axesView();
  if (v && filename)
    writeTGA(QString::fromLocal8Bit(filename), v->grabFramebuffer());
}

void cgxGetTGAScreenShot(int nr)
{
  // Legacy wrote the composited hcpy_<nr>.tga here (and stalled on the
  // composite step when the tools were missing). Same file, direct write.
  char fileName[256];
  std::snprintf(fileName, sizeof(fileName), "hcpy_%d.tga", nr);
  const QImage comp = compositedHardcopy();
  if (!comp.isNull() && !writeTGA(QString::fromLocal8Bit(fileName), comp))
    std::printf("ERROR: could not write %s\n", fileName);
}

void cgxCreateHardcopy(int selection, char *filePtr)
{
  if (!inpformat)
    return;
  HcpyJob job;
  job.selection = selection;
  job.hasFile = (filePtr != nullptr);
  if (filePtr)
    job.file = filePtr;
  job.movieGen = s_movieGen;
  s_jobs.push_back(std::move(job));
  // Fresh pixels for the deferred capture (update() inside a paint only
  // schedules — it never re-enters painting).
  if (CgxMainWindow *m = cgxMainWindow())
  {
    if (auto *g = m->graphicsView())
      g->update();
    if (auto *v = m->menuView())
      v->update();
    if (auto *a = m->axesView())
      a->update();
  }
  if (!s_scheduled)
  {
    s_scheduled = true;
    QTimer::singleShot(0, []() { processHcpyJobs(); });
  }
}
