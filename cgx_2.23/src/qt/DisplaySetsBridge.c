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
extern Faces    *face;
extern char      pickFlag;
extern char      pickfunc[MAX_LINE_LENGTH];
extern char      mode[2];
extern int       setNrbuf;
extern int       pick_zmin;
extern Nodes    *node;
extern Elements *e_enqire;
extern double   *colNr;
extern char      surfFlag;
extern char      drawMode;
extern int       w1, activWindow;
void ConfigureAndShowWindow_Plot(void);
int checkIfNumber(char *string);
extern double    dx_cur, dy_cur;

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
  if (set[i].name[0] == '-') return 0; /* cgx-internal helper sets (-HIGHL, -TMP, ...) */
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


/* ---------------------------------------------------------------------- */
/* Step 19: rename a set (the `rnam` command, with validation). */

#define DS_NAME_MAX 100

static const char *const s_reserved[] = { "all", NSAVE, EDGES, PENETRATION, PLOT2D, THRESHOLD, ETMP, NULL };

static int nameIsReserved(const char *name)
{
  int k;
  for (k = 0; s_reserved[k]; k++)
    if (strcmp(name, s_reserved[k]) == 0) return 1;
  if (strncmp(name, CF, strlen(CF)) == 0) return 1; /* contact faces: +CF<n> */
  return 0;
}

int cgxDsIsProtected(int i)
{
  return cgxDsSetValid(i) ? nameIsReserved(set[i].name) : 1;
}

static void setErr(char *err, int len, const char *msg)
{
  if (err && len > 0) snprintf(err, len, "%s", msg);
}

int cgxDsCheckName(int i, const char *nn, char *err, int errLen)
{
  size_t k, len;
  char buf[MAX_LINE_LENGTH];
  if (!cgxDsSetValid(i)) { setErr(err, errLen, "the set no longer exists"); return 1; }
  if (cgxDsIsProtected(i)) { setErr(err, errLen, "this set is used by cgx by name and cannot be renamed"); return 1; }
  len = nn ? strlen(nn) : 0;
  if (!len) { setErr(err, errLen, "the name is empty"); return 1; }
  if (len > DS_NAME_MAX) { setErr(err, errLen, "the name is too long (max 100 characters)"); return 1; }
  for (k = 0; k < len; k++)
  {
    const unsigned char c = (unsigned char)nn[k];
    if (c <= 32 || c >= 127) { setErr(err, errLen, "only visible ASCII characters without blanks are allowed"); return 1; }
    if (c == '*') { setErr(err, errLen, "'*' is the wildcard of plot/plus and not allowed"); return 1; }
  }
  if (nn[0] == '-') { setErr(err, errLen, "names starting with '-' are reserved for cgx-internal sets"); return 1; }
  if (nameIsReserved(nn)) { setErr(err, errLen, "this name is reserved by cgx"); return 1; }
  snprintf(buf, sizeof buf, "%s", nn);
  if (checkIfNumber(buf) > 0) { setErr(err, errLen, "a pure number is read as a set index and not allowed"); return 1; }
  if (strcmp(nn, set[i].name) == 0) { setErr(err, errLen, "the name is unchanged"); return 1; }
  snprintf(buf, sizeof buf, "%s", nn);
  if (getSetNr(buf) > -1) { setErr(err, errLen, "a set with this name exists already"); return 1; }
  return 0;
}

static void renameUndoRecords(const char *oldName, const char *newName); /* Step 18 data */

int cgxDsRename(int i, const char *nn, char *err, int errLen)
{
  char oldName[MAX_LINE_LENGTH], buf[MAX_LINE_LENGTH];
  if (cgxDsCheckName(i, nn, err, errLen)) return 1;
  snprintf(oldName, sizeof oldName, "%s", set[i].name);
  snprintf(buf, sizeof buf, "%s", nn);
  if (rnam(i, buf) < 0) { setErr(err, errLen, "rename failed (out of memory)"); return 1; }
  printf(" rename %s to %s\n", oldName, set[i].name);
  fflush(stdout);
  renameUndoRecords(oldName, set[i].name);
  return 0;
}

/* ---------------------------------------------------------------------- */
/* Step 18: hide the set(s) of picked entities.
 *
 * Own modal key handler (registered like pick() through glutKeyboardFunc, so
 * the focus hand-off and command-line lock of the port apply unchanged).
 * One GL selection pass collects the picked entities of the chosen type into
 * a plain list - nothing is printed or inserted into sets per hit (that was
 * the cost of reusing qadd/pick/hitAction). Then, set by set:
 *   take the first entity of the list, find the displayed set(s) that contain
 *   it, hide them with the unchanged minus() command, remove every entity of
 *   those sets from the list; repeat until the list is empty. */

typedef struct {
  char name[MAX_LINE_LENGTH];
  char type[MAX_LINE_LENGTH];
  int  col;
  int  width;
  int  batch;
  int  j;        /* index of the blanked pset entry */
  int  nr;       /* its set number */
} UndoRec;

typedef struct { int nr; char t; int j; } Cand;

static int      s_active = 0;
static char     s_mode = 'i';          /* i: closest, a: all in the pick area */
static int      s_ii = 0, s_xcur[2], s_ycur[2];
static UndoRec *s_undo = NULL;
static int      s_nundo = 0;
static int      s_batch = 0;
static GLuint  *s_sel = NULL;
static GLuint   s_selSize = 0;

int cgxDsHidePickActive(void)
{
  return s_active && pickFlag;
}

int cgxDsCanHidePick(void)
{
  return inpformat && !pickFlag && !s_active;
}

static int entArray(int s, char t, int **p, int *n)
{
  switch (t)
  {
    case 'n': *p = set[s].node; *n = set[s].anz_n; return 1;
    case 'e': *p = set[s].elem; *n = set[s].anz_e; return 1;
    case 'f': *p = set[s].face; *n = set[s].anz_f; return 1;
    case 'p': *p = set[s].pnt;  *n = set[s].anz_p; return 1;
    case 'l': *p = set[s].line; *n = set[s].anz_l; return 1;
    case 's': *p = set[s].surf; *n = set[s].anz_s; return 1;
    case 'b': *p = set[s].body; *n = set[s].anz_b; return 1;
    case 'S': *p = set[s].nurs; *n = set[s].anz_nurs; return 1;
    case 'L': *p = set[s].nurl; *n = set[s].anz_nurl; return 1;
  }
  *p = NULL; *n = 0;
  return 0;
}

/* ---- the one GL selection pass (same drawing as goPicking(), no processHits) ---- */
static int ensureSelBuf(void)
{
  double dum = (double)anz->n + anz->e + anz->f + anz->g + anz->t + anzGeo->p + anzGeo->l +
               anzGeo->s + anzGeo->b + anzGeo->sh + anzGeo->nurl + anzGeo->nurs + 1000;
  GLuint want;
  dum *= 10.;
  want = (dum > 16000000.) ? 16000000u : (GLuint)dum;
  if (want < 1000) want = 1000;
  if (s_selSize < want)
  {
    GLuint *p = (GLuint *)realloc(s_sel, want * sizeof(GLuint));
    if (!p) return 0;
    s_sel = p;
    s_selSize = want;
  }
  return 1;
}

static void drawForSelection(char t)
{
  int j;
  if (drawMode == 4)
  {
    drawSets(PICK);
    return;
  }
  if (surfFlag == 1)
  {
    for (j = 0; j < anzGeo->psets; j++)
    {
      if (pset[j].type[0] != 'f') continue;
      if (t == 'e') drawFaces_plot(set[pset[j].nr].anz_f, set[pset[j].nr].face, node, colNr, face, 2, 'e', 1, PICK);
      else if (t == 'f') drawFaces_plot(set[pset[j].nr].anz_f, set[pset[j].nr].face, node, colNr, face, 2, 0, 1, PICK);
      else if (t == 'n') drawFaceNodes_plot(set[pset[j].nr].anz_f, set[pset[j].nr].face, node, face, 2, 0);
    }
  }
  else
  {
    for (j = 0; j < anzGeo->psets; j++)
    {
      if (t == 'e' && pset[j].type[0] == 'e')
        drawElements_plot(set[pset[j].nr].anz_e, set[pset[j].nr].elem, node, colNr, e_enqire, 2, 0, 1, PICK);
      else if (t == 'f' && pset[j].type[0] == 'f')
        drawFaces_plot(set[pset[j].nr].anz_f, set[pset[j].nr].face, node, colNr, face, 2, 0, 1, PICK);
      else if (t == 'n' && pset[j].type[0] == 'e')
        drawElemNodes_plot(set[pset[j].nr].anz_e, set[pset[j].nr].elem, node, e_enqire, 2, 0);
    }
  }
}

/* picked entities of type t under (x,y): returns their number, list malloc'ed
   (sorted, unique); *zmin = nearest depth among them. */
static int selectEntities(char t, int x, int y, int closestOnly, int **out, unsigned int *zmin)
{
  GLint viewport[4];
  GLint hits;
  int i, n = 0, cap = 0;
  int *list = NULL;
  const GLuint *p;
  unsigned int zbest = 0xffffffffu;
  int bestEnt = -1;

  *out = NULL; *zmin = 0xffffffffu;
  if (!ensureSelBuf()) return 0;
  glutSetWindow(w1);
  glGetIntegerv(GL_VIEWPORT, viewport);
  glSelectBuffer(s_selSize, s_sel);
  glRenderMode(GL_SELECT);
  glInitNames();
  glPushName(-1);
  glLoadIdentity();
  gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y), dx_cur, dy_cur, viewport);
  moveModel();
  drawForSelection(t);
  glFlush();
  hits = glRenderMode(GL_RENDER);
  if (hits < 0)
  {
    printf("\nWARNING: Overflow occured, select a smaller region and try again!\n");
    return 0;
  }
  p = s_sel;
  for (i = 0; i < hits; i++)
  {
    const GLuint nn = p[0];
    if (nn >= 2 && p[3] == (GLuint)(unsigned char)t)
    {
      if (closestOnly)
      {
        if (p[1] <= zbest) { zbest = p[1]; bestEnt = (int)p[4]; }
      }
      else
      {
        if (n == cap)
        {
          int *q;
          cap = cap ? 2 * cap : 256;
          q = (int *)realloc(list, cap * sizeof(int));
          if (!q) { free(list); return 0; }
          list = q;
        }
        list[n++] = (int)p[4];
        if (p[1] < *zmin) *zmin = p[1];
      }
    }
    p += 3 + nn;
  }
  if (closestOnly)
  {
    if (bestEnt < 0) return 0;
    list = (int *)malloc(sizeof(int));
    if (!list) return 0;
    list[0] = bestEnt;
    *zmin = zbest;
    *out = list;
    return 1;
  }
  *out = list;
  return n;
}

static int cmpInt(const void *a, const void *b)
{
  const int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

static int sortUnique(int *a, int n)
{
  int i, m = 0;
  if (n < 2) return n;
  qsort(a, n, sizeof(int), cmpInt);
  for (i = 0; i < n; i++)
    if (!m || a[m - 1] != a[i]) a[m++] = a[i];
  return m;
}

/* ---- membership of a picked entity in a displayed set entry ---- */
typedef struct { int *e; int n; int built; } ElemCache;

static int memb(const Cand *c, char T, int x, ElemCache *cache)
{
  int *a, na;
  if (c->t == T)
  {
    entArray(c->nr, T, &a, &na);
    return na > 0 && getIndex(&a, na, x) > -1;
  }
  if (T == 'f' && c->t == 'e')
  {
    a = set[c->nr].elem; na = set[c->nr].anz_e;
    return na > 0 && getIndex(&a, na, face[x].elem_nr) > -1;
  }
  if (T == 'e' && c->t == 'f')
  {
    /* element of a face-displayed set: elements owning at least one of its faces */
    ElemCache *ec = &cache[c->j];
    if (!ec->built)
    {
      int k;
      ec->built = 1;
      ec->n = 0;
      ec->e = (int *)malloc((set[c->nr].anz_f + 1) * sizeof(int));
      if (ec->e)
      {
        for (k = 0; k < set[c->nr].anz_f; k++) ec->e[k] = face[set[c->nr].face[k]].elem_nr;
        ec->n = sortUnique(ec->e, set[c->nr].anz_f);
      }
    }
    return ec->n > 0 && getIndex(&ec->e, ec->n, x) > -1;
  }
  return 0;
}

static void renameUndoRecords(const char *oldName, const char *newName)
{
  int k;
  for (k = 0; k < s_nundo; k++)
    if (strcmp(s_undo[k].name, oldName) == 0) snprintf(s_undo[k].name, MAX_LINE_LENGTH, "%s", newName);
}

/* ---- the set-by-set loop ---- */
static int hideLoop(char T, int *list, int n)
{
  int start = 0, nsets = 0, k, j;
  ElemCache *cache = (ElemCache *)calloc(anzGeo->psets + 1, sizeof(ElemCache));
  Cand *hid = NULL; int nhid = 0, caphid = 0;
  char rec[MAX_LINE_LENGTH];

  if (!cache) return 0;
  s_batch++;
  while (start < n)
  {
    const int x = list[start];
    int ncand = 0, found = 0;
    Cand cand[256];
    /* displayed sets that contain the first entity of the list */
    for (j = 0; j < anzGeo->psets && ncand < 256; j++)
    {
      Cand c;
      c.nr = pset[j].nr; c.t = pset[j].type[0]; c.j = j;
      if (c.t == ' ' || c.t == 0 || c.nr < 0 || c.nr >= anz->sets || !cgxDsSetValid(c.nr)) continue;
      if (!memb(&c, T, x, cache)) continue;
      cand[ncand++] = c;
    }
    if (!ncand) { start++; continue; } /* not in any displayed set: drop it */

    for (k = 0; k < ncand; k++)
    {
      int dup = 0, m;
      for (m = 0; m < k; m++) if (cand[m].nr == cand[k].nr && cand[m].t == cand[k].t) dup = 1;
      if (dup) continue;
      /* undo records for every pset entry minus() is going to blank */
      for (j = 0; j < anzGeo->psets; j++)
        if (pset[j].nr == cand[k].nr && pset[j].type[0] == cand[k].t)
        {
          UndoRec *u = (UndoRec *)realloc(s_undo, (s_nundo + 1) * sizeof(UndoRec));
          if (!u) continue;
          s_undo = u;
          snprintf(s_undo[s_nundo].name, MAX_LINE_LENGTH, "%s", set[cand[k].nr].name);
          snprintf(s_undo[s_nundo].type, MAX_LINE_LENGTH, "%s", pset[j].type);
          s_undo[s_nundo].col = pset[j].col;
          s_undo[s_nundo].width = pset[j].width;
          s_undo[s_nundo].batch = s_batch;
          s_undo[s_nundo].j = j;
          s_undo[s_nundo].nr = cand[k].nr;
          s_nundo++;
          /* what minus() does to the entry; minus() itself rebuilds all display
             lists once per displayed entry (minutes for hundreds of sets), so
             the lists are rebuilt once after the whole pick instead */
          strcpy(pset[j].type, "    ");
        }
      snprintf(rec, sizeof rec, "%c %s", cand[k].t, set[cand[k].nr].name);
      printf(" minus %s\n", rec);
      nsets++;
      if (nhid == caphid)
      {
        Cand *q = (Cand *)realloc(hid, (caphid = caphid ? 2 * caphid : 16) * sizeof(Cand));
        if (!q) break;
        hid = q;
      }
      hid[nhid++] = cand[k];
    }
    /* remove all entities of the hidden sets from the rest of the list */
    {
      int w = start;
      for (k = start; k < n; k++)
      {
        int inHidden = 0, m;
        for (m = 0; m < nhid && !inHidden; m++) inHidden = memb(&hid[m], T, list[k], cache);
        if (!inHidden) list[w++] = list[k];
      }
      n = w;
      /* the first entity is in a hidden set by construction; make sure the loop ends */
      if (n > start && list[start] == x) start++;
    }
    nhid = 0; /* the sets hidden so far have no entities left in the list */
  }
  for (j = 0; j < anzGeo->psets + 1; j++) free(cache[j].e);
  free(cache);
  free(hid);
  if (!nsets) s_batch--;
  else
  {
    updateDispLists();
    ConfigureAndShowWindow_Plot();
  }
  return nsets;
}

static void undoHidePick(void)
{
  int n = 0, last, restored = 0;
  char rec[MAX_LINE_LENGTH];
  if (!s_nundo) { printf(" hide set: nothing to undo\n"); return; }
  last = s_undo[s_nundo - 1].batch;
  while (s_nundo && s_undo[s_nundo - 1].batch == last)
  {
    UndoRec *u = &s_undo[s_nundo - 1];
    const int nr = getSetNr(u->name);
    if (nr >= 0)
    {
      /* normal case: the blanked entry is still there, put it back in place */
      if (u->j >= 0 && u->j < anzGeo->psets && pset[u->j].nr == nr && pset[u->j].type[0] == ' ' &&
          u->nr == nr)
      {
        strcpy(pset[u->j].type, u->type);
        pset[u->j].col = u->col;
        pset[u->j].width = u->width;
        printf(" plus %c %s\n", u->type[0], u->name);
        restored = 1;
        n++;
      }
      else
      {
        /* the pset list changed meanwhile (plot, deleted sets): fall back to plus */
        if (u->col >= 0 && u->col < entitycols)
        {
          if (u->width > 0) snprintf(rec, sizeof rec, "%s %s %s %d", u->type, u->name, entitycol[u->col].name, u->width);
          else              snprintf(rec, sizeof rec, "%s %s %s", u->type, u->name, entitycol[u->col].name);
        }
        else snprintf(rec, sizeof rec, "%s %s", u->type, u->name);
        printf(" plus %s\n", rec);
        plus(rec);
        n++;
      }
    }
    s_nundo--;
  }
  s_batch = s_nundo ? s_undo[s_nundo - 1].batch : 0;
  if (restored)
  {
    drawMode = 4;
    updateDispLists();
    ConfigureAndShowWindow_Plot();
  }
  printf(" hide set: %d set(s) shown again\n", n);
  fflush(stdout);
}

static void endHidePick(void)
{
  if (!s_active) return;
  s_active = 0;
  glutSetWindow(w1);
  glutKeyboardFunc(Keyboard);
  glutSetWindow(activWindow);
  pickFlag = 0;
  dx_cur = PICK_LENGTH; dy_cur = PICK_LENGTH;
  s_ii = 0;
  free(s_sel); s_sel = NULL; s_selSize = 0;
  printf(" done\n");
  fflush(stdout);
  glutPostRedisplay();
}

static void hidePickKey(unsigned char key, int x, int y)
{
  int *list = NULL, n;
  unsigned int z;
  if (key == 27) key = 'q';
  switch (key)
  {
    case 'q': case 'Q':
      endHidePick();
      return;
    case 'u':
      undoHidePick();
      glutPostRedisplay();
      return;
    case 'a':
      s_mode = 'a';
      printf("mode:a\n");
      return;
    case 'i':
      s_mode = 'i';
      dx_cur = PICK_LENGTH; dy_cur = PICK_LENGTH;
      printf("mode:i\n");
      return;
    case 'r':
      s_xcur[s_ii] = x; s_ycur[s_ii] = y;
      if (s_ii)
      {
        s_ii = 0;
        dx_cur = fabs((double)(s_xcur[1] - s_xcur[0]));
        dy_cur = fabs((double)(s_ycur[1] - s_ycur[0]));
        printf(" cursorArea set to dx_cur=%lf dy_cur=%lf\n", dx_cur, dy_cur);
      }
      else s_ii = 1;
      glutPostRedisplay();
      return;
    case 'n': case 'e': case 'f': case 'p': case 'l': case 's': case 'b': case 'S': case 'L':
      break;
    default:
      return; /* other qadd keys (max/min, text, ...) make no sense here */
  }
  n = selectEntities((char)key, x, y, s_mode == 'i', &list, &z);
  if (n > 0)
  {
    n = sortUnique(list, n);
    n = hideLoop((char)key, list, n);
    if (n) printf(" hide set: %d set(s) hidden, 'u' shows them again\n", n);
    else   printf(" hide set: the picked entities are in no displayed set\n");
  }
  else printf(" hide set: nothing picked\n");
  free(list);
  fflush(stdout);
  glutPostRedisplay();
}

int cgxDsHidePickBegin(void)
{
  if (!cgxDsCanHidePick())
  {
    if (inpformat) printf(" hide set: finish the active selection first (q)\n");
    return 0;
  }
  s_nundo = 0; s_batch = 0;
  s_mode = 'i'; s_ii = 0;
  s_active = 1;
  pickFlag = 1;
  mode[0] = 'i';
  strcpy(pickfunc, "qhid");
  glutSetWindow(w1);
  glutKeyboardFunc(hidePickKey);
  glutSetWindow(activWindow);
  printf(" hide set by picking: put the cursor on an entity and press its type key\n"
         "   n e f p l s b S L  (i: closest, a: all in the pick area, r r: set the area,\n"
         "   u: undo, q or Esc: end)\n");
  fflush(stdout);
  return 1;
}

int cgxDsHidePickAt(int x, int y)
{
  /* bulkier entities first: on equal depth the bulkier one wins */
  static const char prio[CGX_DS_NTYPES] = { 'b', 'e', 'f', 's', 'S', 'L', 'l', 'p', 'n' };
  char order[CGX_DS_NTYPES];
  int norder = 0, j, k, t, nset;
  char best = 0;
  int bestEnt = -1;
  unsigned int bestZ = 0xffffffffu;
  const unsigned int tol = 0xffffffffu / 100u; /* within 1% of the depth range = same depth */

  if (!inpformat) return 0;
  if (!cgxDsCanHidePick())
  {
    printf(" hide set: finish the active selection first (q)\n");
    return 0;
  }
  for (k = 0; k < CGX_DS_NTYPES; k++)
    for (j = 0; j < anzGeo->psets; j++)
    {
      const int nr = pset[j].nr;
      if (pset[j].type[0] != prio[k] || nr < 0 || nr >= anz->sets || !cgxDsSetValid(nr)) continue;
      order[norder++] = prio[k];
      break;
    }
  if (!norder) { printf(" hide set: no set is displayed\n"); return 0; }

  s_nundo = 0; s_batch = 0;
  dx_cur = PICK_LENGTH * 2; dy_cur = PICK_LENGTH * 2;
  for (t = 0; t < norder; t++)
  {
    int *l = NULL;
    unsigned int z;
    if (selectEntities(order[t], x, y, 1, &l, &z) > 0)
    {
      if (!best || z + tol < bestZ) { best = order[t]; bestZ = z; bestEnt = l[0]; }
    }
    free(l);
  }
  dx_cur = PICK_LENGTH; dy_cur = PICK_LENGTH;
  free(s_sel); s_sel = NULL; s_selSize = 0;
  if (!best)
  {
    printf(" hide set: no displayed entity under the cursor\n");
    fflush(stdout);
    return 0;
  }
  nset = hideLoop(best, &bestEnt, 1);
  if (nset) printf(" hide set: %d set(s) hidden\n", nset);
  fflush(stdout);
  glutPostRedisplay();
  return nset > 0;
}
