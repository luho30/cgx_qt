#pragma once
/* Step 16: C bridge for the "Display Sets" menu (see DisplaySetsBridge.c).
 * Plain C types only, safe to include from C and C++. Entity type index:
 * 0 n(odes) 1 e(lements) 2 f(aces) 3 p(oints) 4 l(ines) 5 s(urfaces)
 * 6 b(odies) 7 S (nurbs surfaces) 8 L (nurbs lines) */
#ifdef __cplusplus
extern "C" {
#endif

#define CGX_DS_NTYPES 9

char cgxDsTypeLetter(int t);
int cgxDsSetSlots(void);              /* upper bound of the set index; 0 if no model */
int cgxDsSetValid(int i);             /* 1 for a real, live, non-sequence set */
const char *cgxDsSetName(int i);
int cgxDsEntityCount(int i, int t);   /* entities of type t contained in set i */
void cgxDsSetColor(int i, float *r, float *g, float *b);
int cgxDsDisplayedMask(int i);        /* bit t set: type t of set i is on screen */
/* 64-bit hash of everything the menu shows (sets, names, counts, pset entries);
   changes whenever a set is created/deleted/modified or the display changes */
unsigned long long cgxDsSignature(void);
/* ---- Step 19: rename ---- */
int cgxDsIsProtected(int i);            /* 1: cgx looks this set up by name (all, :NOSAVE, +EDGE, ...) */
/* validate a new set name; 0 = ok, otherwise err holds a one-line reason */
int cgxDsCheckName(int i, const char *newName, char *err, int errLen);
/* rename set i through cgx's own rnam(); 0 = ok, else err filled and nothing changed */
int cgxDsRename(int i, const char *newName, char *err, int errLen);
int cgxDsShow(int i, int t);          /* legacy `plus <t> <set> <col>` (no-op if shown) */
int cgxDsHide(int i, int t);          /* legacy `minus <t> <set>` (no-op if hidden) */

/* ---- Step 18: hide the set(s) of a picked entity ---- */
int cgxDsCanHidePick(void);             /* 1 if a hide-pick can start now */
int cgxDsHidePickBegin(void);           /* modal pick (menu entry), 1 if started */
int cgxDsHidePickActive(void);          /* 1 while that modal pick runs */
int cgxDsHidePickAt(int x, int y);      /* one-shot (Ctrl+click), 1 if sets were hidden */

#ifdef __cplusplus
}
#endif
