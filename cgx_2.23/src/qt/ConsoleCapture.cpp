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

#include "ConsoleCapture.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include <pthread.h>
#include <unistd.h>

namespace
{
const size_t kMaxPending = 8u * 1024u * 1024u; // GUI never polled: bound memory

std::mutex s_mutex;
std::string s_pending;
unsigned long long s_captured = 0;
unsigned long long s_dropped = 0;

int s_savedOut = -1; // the original stdout
int s_pipeRead = -1;
std::thread s_thread;
bool s_installed = false;
bool s_restored = false;

void writeAll(int fd, const char *data, size_t n)
{
  while (n > 0)
  {
    const ssize_t w = ::write(fd, data, n);
    if (w < 0)
    {
      if (errno == EINTR)
        continue;
      return; // EPIPE etc.: original consumer is gone, keep capturing anyway
    }
    data += w;
    n -= (size_t)w;
  }
}

void readerLoop()
{
  // If the original stdout is a closed pipe (`cgx | head`), write() must fail
  // with EPIPE instead of killing the process from this thread.
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);

  char buf[65536];
  for (;;)
  {
    const ssize_t n = ::read(s_pipeRead, buf, sizeof(buf));
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      break;
    }
    if (n == 0)
      break; // all write ends closed (restore at exit)
    writeAll(s_savedOut, buf, (size_t)n); // tee: terminal / log unchanged
    std::lock_guard<std::mutex> lock(s_mutex);
    s_pending.append(buf, (size_t)n);
    s_captured += (unsigned long long)n;
    if (s_pending.size() > kMaxPending)
    {
      const size_t drop = s_pending.size() - kMaxPending;
      s_pending.erase(0, drop);
      s_dropped += drop;
    }
  }
}

void restoreAtExit()
{
  if (s_restored || !s_installed)
    return;
  s_restored = true;
  std::fflush(stdout);
  ::dup2(s_savedOut, STDOUT_FILENO); // drops the last write end of the pipe
  if (s_thread.joinable())
    s_thread.join(); // reader sees EOF after draining everything
  ::close(s_pipeRead);
  ::close(s_savedOut);
}
} // namespace

namespace cgxconsole
{
void install()
{
  if (s_installed)
    return;
  int fds[2];
  if (::pipe(fds) != 0)
    return; // capture is a convenience: never block startup
  s_savedOut = ::dup(STDOUT_FILENO);
  if (s_savedOut < 0)
  {
    ::close(fds[0]);
    ::close(fds[1]);
    return;
  }
  std::fflush(stdout);
  ::dup2(fds[1], STDOUT_FILENO);
  ::close(fds[1]); // fd 1 is now the only write end
  s_pipeRead = fds[0];
  // Terminal behaviour: a pipe would otherwise make stdout fully buffered and
  // the panel would only update every 4 KB.
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  s_installed = true;
  s_thread = std::thread(readerLoop);
  std::atexit(restoreAtExit);
}

QString takeNew()
{
  std::string chunk;
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    chunk.swap(s_pending);
  }
  if (chunk.empty())
    return QString();
  // Drop control characters the console cannot show; keep \n \r \b \t.
  std::string clean;
  clean.reserve(chunk.size());
  for (unsigned char c : chunk)
  {
    if (c >= 0x20 || c == '\n' || c == '\r' || c == '\b' || c == '\t')
      clean.push_back((char)c);
  }
  return QString::fromUtf8(clean.data(), (qsizetype)clean.size());
}

unsigned long long bytesCaptured()
{
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_captured;
}

unsigned long long bytesDropped()
{
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_dropped;
}
} // namespace cgxconsole
