#if !defined(OBJECTSELECTION)
#define OBJECTSELECTION

#include "Common.h"
#include "DynExt.h"

typedef headerObject object;

object * GetSingleInstace(LPRDATA rdPtr, short Oi);
bool FilterObjects(LPRDATA rdPtr, short Oi, bool (*filterFunction)(LPRDATA, LPRO, void *),
                   void * userdata, bool is_negated);

#endif // !defined(OBJECTSELECTION)