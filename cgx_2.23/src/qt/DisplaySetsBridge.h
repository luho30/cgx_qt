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
int cgxDsShow(int i, int t);          /* legacy `plus <t> <set> <col>` (no-op if shown) */
int cgxDsHide(int i, int t);          /* legacy `minus <t> <set>` (no-op if hidden) */

#ifdef __cplusplus
}
#endif
