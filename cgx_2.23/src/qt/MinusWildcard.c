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

/* Step 20: wildcards for `minus` (as `plus` and `plot` have them).
 *
 * CMake compiles cgx.c with -Dminus=cgx_minus_legacy, so the upstream
 * function keeps its exact body under that name and every caller of minus()
 * (command interpreter, qenq max/min mode, the Display Sets bridge) lands
 * here. Without a '*' in the set name the call is passed through unchanged.
 * With a '*' the pattern is matched against the sets that are on the screen
 * with the same rules as wildcard_plot() (cgx.c), so `minus e P` removes
 * exactly what `plus e P` displayed. */

#include <cgx.h>

extern Sets      *set;
extern Psets     *pset;
extern Summen     anz[1];
extern SumGeo     anzGeo[1];
extern char       inpformat;

int cgx_minus_legacy( char *record );
void ConfigureAndShowWindow_Plot( void );

/* same matching as wildcard_plot(): every '*'-separated piece must occur in
   the name; a first/last piece not bordered by '*' must sit at start/end
   (only the first/last character is compared, exactly like plus) */
static int wildcardMatches(const char *pattern, char **dat, int length, const char *setname)
{
  int i, found = 0;
  for (i = 0; i < length; i++)
  {
    if (strstr(setname, dat[i]) != NULL)
    {
      found++;
      if (i == 0 && pattern[0] != '*' && pattern[0] != setname[0]) found--;
      if (i == length - 1 && pattern[strlen(pattern) - 1] != '*' &&
          pattern[strlen(pattern) - 1] != setname[strlen(setname) - 1]) found--;
    }
  }
  return found == length;
}

int minus( char *record )
{
  char typ[MAX_LINE_LENGTH], setnam[MAX_LINE_LENGTH];
  char **dat = NULL;
  int length, i, j, removed = 0, nsets = 0;
  char *seen;

  if (!inpformat) return cgx_minus_legacy(record);

  typ[0] = setnam[0] = 0;
  length = sword(record, typ);
  if (length < 0) return cgx_minus_legacy(record);
  length = 1 + length + sword(&record[length], setnam);
  if (setnam[0] == 0 || strchr(setnam, '*') == NULL) return cgx_minus_legacy(record);
  if (typ[0] == 'm') typ[0] = 'e';

  length = strsplt(setnam, '*', &dat);
  if (length <= 0)
  {
    /* only '*' characters: a bare `*` matches every set (like plus) */
    length = 0;
  }

  seen = (char *)calloc(anz->sets + 1, 1);
  if (!seen) { for (i = 0; i < length; i++) free(dat[i]); free(dat); return cgx_minus_legacy(record); }

  for (j = 0; j < anzGeo->psets; j++)
  {
    const int nr = pset[j].nr;
    if (pset[j].type[0] != typ[0] || nr < 1 || nr >= anz->sets) continue; /* set 0 = all */
    if (set[nr].name == (char *)NULL || set[nr].type != 0 || set[nr].name[0] == '-') continue;
    if (length > 0 && !wildcardMatches(setnam, dat, length, set[nr].name)) continue;
    printf (" set:%s type:%s removed\n", set[nr].name, pset[j].type);
    strcpy( pset[j].type, "    ");
    removed++;
    if (!seen[nr]) { seen[nr] = 1; nsets++; }
  }
  for (i = 0; i < length; i++) free(dat[i]);
  free(dat);
  free(seen);

  if (!removed)
  {
    printf (" minus: no displayed set of type %c matches '%s'\n", typ[0], setnam);
    return 1;
  }
  printf (" minus %c %s: %d set(s) removed\n", typ[0], setnam, nsets);
  /* the legacy minus() rebuilds all display lists per displayed entry; once is enough */
  updateDispLists();
  ConfigureAndShowWindow_Plot();
  return 1;
}
