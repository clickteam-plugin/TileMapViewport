#include "Common.h"
#include "DynExt.h"

int oiListItemSize = 0;
void InitOiListItemSize(LPMV pMv)
{
    oiListItemSize = sizeof(objInfoList);
    if (pMv->mvCallFunction(NULL, EF_ISUNICODE, (LPARAM)0, (LPARAM)0, (LPARAM)0))
        oiListItemSize += 24;
#ifndef HWABETA
    if (pMv->mvCallFunction(NULL, EF_ISHWA, (LPARAM)0, (LPARAM)0, (LPARAM)0))
        oiListItemSize += sizeof(LPVOID);
#endif
}

////

double getAlterableValue(LPRO object, int ValueNum)
{
    rVal * object_rVal = &object->rov;

    if (object_rVal->rvpValues) {
        CValue & val = object_rVal->rvpValues[ValueNum];
        if (val.m_type == TYPE_LONG) {
            return (double)val.m_long;
        }
        else if (val.m_type == TYPE_DOUBLE) {
            return val.m_double;
        }
    }

    // unsupported version, value type or no alt values present.
    return 0;
}

/*double getAlterableValue(LPRO object, int ValueNum)
{
        DWORD MMFVersion =
object->roHo.hoAdRunHeader->rh4.rh4Mv->mvGetVersion();

        rVal* object_rVal = &object->rov;

        if (((MMFVersion & MMFVERSION_MASK) <= MMFVERSION_20) && ((MMFVersion &
MMFBUILD_MASK) < 243))
        {
                if (object_rVal->rvValuesType[ValueNum] == TYPE_LONG)
                {
                        return (double)object_rVal->rvValues[ValueNum];
                }
                else if (object_rVal->rvValuesType[ValueNum] == TYPE_FLOAT)
                {
                        return
(double)UNPACK_FLOAT(object_rVal->rvValues[ValueNum]);
                }
        }
        else if (((MMFVersion & MMFVERSION_MASK) >= MMFVERSION_20) &&
((MMFVersion & MMFBUILD_MASK) >= 243))
        {
                if (object_rVal->rvValues[0])
                {
                        AltVal & val =
((AltVal*)(object_rVal->rvValues[0]))[ValueNum];
                        if (val.ValueType == TYPE_LONG)
                        {
                                return (double)val.LongValue;
                        }
                        else if (val.ValueType == TYPE_FLOAT)
                        {
                                return val.DoubleValue;
                        }
                }
        }

        // unsupported version, value type or no alt values present.
        return 0;
}*/
