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

/* Step 16: C-side bridge for the "Display Sets" menu (qt/DisplaySets.cpp).
 *
 * cgx.h has no extern "C" guards and pulls in the glut shim macros, so the
 * Qt (C++) side never includes it. This file is the only place that touches
 * the legacy set / pset (= "displayed sets") structures; it drives the
 * unchanged plus()/minus() commands, exactly what typing them would do. */

#include <cgx.h>
#include "DisplaySetsBridge.h"

extern Sets     *set;
extern Psets    *pset;
extern Summen    anz[1];
extern SumGeo    anzGeo[1];
extern Entitycol *entitycol;
extern int       entitycols;
extern char      inpformat;

static const char s_letters[CGX_DS_NTYPES] = { 'n', 'e', 'f', 'p', 'l', 's', 'b', 'S', 'L' };

char cgxDsTypeLetter(int t)
{
  if (t < 0 || t >= CGX_DS_NTYPES) return 0;
  return s_letters[t];
}

int cgxDsSetSlots(void)
{
  if (!inpformat || !set) return 0;
  return anz->sets;
}

int cgxDsSetValid(int i)
{
  if (i < 0 || i >= cgxDsSetSlots()) return 0;
  if (set[i].name == (char *)NULL) return 0;
  if (set[i].type != 0) return 0; /* ordered sequences (qseq) are no sets */
  return 1;
}

const char *cgxDsSetName(int i)
{
  return cgxDsSetValid(i) ? set[i].name : "";
}

int cgxDsEntityCount(int i, int t)
{
  if (!cgxDsSetValid(i)) return 0;
  switch (t)
  {
    case 0: return set[i].anz_n;
    case 1: return set[i].anz_e;
    case 2: return set[i].anz_f;
    case 3: return set[i].anz_p;
    case 4: return set[i].anz_l;
    case 5: return set[i].anz_s;
    case 6: return set[i].anz_b;
    case 7: return set[i].anz_nurs;
    case 8: return set[i].anz_nurl;
  }
  return 0;
}

/* stable colour index per set, same palette plot e * cycles through */
static int setColorIndex(int i)
{
  if (entitycols <= 3) return 0;
  return 3 + (i % (entitycols - 3));
}

void cgxDsSetColor(int i, float *r, float *g, float *b)
{
  const int c = setColorIndex(i);
  *r = *g = *b = 0.f;
  if (!entitycol || c >= entitycols) return;
  *r = entitycol[c].r;
  *g = entitycol[c].g;
  *b = entitycol[c].b;
}

int cgxDsDisplayedMask(int i)
{
  int j, t, mask = 0;
  if (!cgxDsSetValid(i) || !pset) return 0;
  for (j = 0; j < anzGeo->psets; j++)
  {
    if (pset[j].nr != i) continue;
    for (t = 0; t < CGX_DS_NTYPES; t++)
      if (pset[j].type[0] == s_letters[t]) mask |= 1 << t;
  }
  return mask;
}

int cgxDsShow(int i, int t)
{
  char rec[MAX_LINE_LENGTH];
  const int c = setColorIndex(i);
  if (!cgxDsSetValid(i) || t < 0 || t >= CGX_DS_NTYPES) return -1;
  if (!entitycol || c >= entitycols) return -1;
  if (cgxDsDisplayedMask(i) & (1 << t)) return 0; /* already visible */
  if (t == 0 || t == 3) /* nodes and points: width 5, like `plot n *` */
    snprintf(rec, sizeof rec, "%c %s %s 5", s_letters[t], set[i].name, entitycol[c].name);
  else
    snprintf(rec, sizeof rec, "%c %s %s", s_letters[t], set[i].name, entitycol[c].name);
  printf(" plus %s\n", rec);
  return plus(rec);
}

int cgxDsHide(int i, int t)
{
  char rec[MAX_LINE_LENGTH];
  if (!cgxDsSetValid(i) || t < 0 || t >= CGX_DS_NTYPES) return -1;
  if (!(cgxDsDisplayedMask(i) & (1 << t))) return 0;
  snprintf(rec, sizeof rec, "%c %s", s_letters[t], set[i].name);
  printf(" minus %s\n", rec);
  return minus(rec);
}

static unsigned long long fnv(unsigned long long h, unsigned long long v)
{
  int k;
  for (k = 0; k < 8; k++)
  {
    h ^= (v >> (8 * k)) & 0xffu;
    h *= 1099511628211ULL;
  }
  return h;
}

unsigned long long cgxDsSignature(void)
{
  unsigned long long h = 1469598103934665603ULL;
  int i, t, j;
  const char *p;
  h = fnv(h, (unsigned long long)cgxDsSetSlots());
  for (i = 0; i < cgxDsSetSlots(); i++)
  {
    if (!cgxDsSetValid(i)) continue;
    h = fnv(h, (unsigned long long)i);
    for (p = set[i].name; *p; p++) h = fnv(h, (unsigned char)*p);
    for (t = 0; t < CGX_DS_NTYPES; t++) h = fnv(h, (unsigned long long)cgxDsEntityCount(i, t));
  }
  if (inpformat && pset)
  {
    h = fnv(h, (unsigned long long)anzGeo->psets);
    for (j = 0; j < anzGeo->psets; j++)
    {
      h = fnv(h, (unsigned long long)pset[j].nr);
      h = fnv(h, (unsigned char)pset[j].type[0]);
      h = fnv(h, (unsigned char)pset[j].type[1]);
      h = fnv(h, (unsigned char)pset[j].type[2]);
      h = fnv(h, (unsigned long long)pset[j].col);
    }
  }
  return h;
}
