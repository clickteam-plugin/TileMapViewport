#ifndef __DynExt_h__
#define __DynExt_h__

#pragma pack(push, 1)

// Make identifier
//#define MAKEID(a,b,c,d) ((#@a << 24)|(#@b << 16)|(#@c << 8)|(#@d))

#define IS_SET(flags, flag) (((flags) & (flag)) ? true : false)
#define PACK_FLOAT(f) *((long *)&(f))
#define UNPACK_FLOAT(f) *((float *)&(f))

////

template <class T> inline T * PtrAddBytes(T * ptr, int bytes)
{
    return (T *)((__int8 *)ptr + bytes);
}

// HWA/Unicode handling, courtesy of Yves
#define EF_ISHWA 112 // Returns TRUE if HWA version (editor and runtime)
#define EF_ISUNICODE                                                           \
    113 // Returns TRUE if the editor or runtime is in Unicode mode
#define EF_GETAPPCODEPAGE 114 // Returns the code page of the application

extern int oiListItemSize;
void InitOiListItemSize(LPMV pMv);
#define GetOILPtr(oiListPtr, oiListIndex)                                      \
    PtrAddBytes(oiListPtr, oiListItemSize * oiListIndex)
#define NextOILPtr(oiListItemPtr) PtrAddBytes(oiListItemPtr, oiListItemSize)

// Convert a fixed value to an object
inline headerObject * Fixed2Object(LPRDATA rdPtr, unsigned int fixed)
{
    LPOBL ObjectList = rdPtr->rHo.hoAdRunHeader->rhObjectList;
    headerObject * pObject = ObjectList[0x0000FFFF & fixed].oblOffset;
    if (pObject && pObject->hoCreationId == fixed >> 16) {
        return pObject;
    }
    else {
        return NULL;
    }
}

// Convert an object to a fixed value
inline unsigned int Object2Fixed(headerObject * object)
{
    return (object->hoCreationId << 16) + object->hoNumber;
}

inline long Float2Long(float value) { return *(long *)&value; }

inline float Long2Float(long value) { return *(float *)&value; }

////

double getAlterableValue(LPRO object, int ValueNum);

/*struct AltVal
{
        long ValueType;
        long ValueFlags;
        union
        {
                long LongValue;
                double DoubleValue;
        };
};

double getAlterableValue(LPRO object, int ValueNum);
*/

////

enum Comparison {
    PC_EQUAL = CMPOPE_EQU,
    PC_DIFFERENT = CMPOPE_DIF,
    PC_LOWER_OR_EQUAL = CMPOPE_LOWEQU,
    PC_LOWER = CMPOPE_LOW,
    PC_GREATER_OR_EQUAL = CMPOPE_GREEQU,
    PC_GREATER = CMPOPE_GRE,
};

enum CompValueType {
    PC_LONG = 0,
    PC_FLOAT = 23,
    PC_DOUBLE = 23,
};

struct ParamComp {
    //	Comparison compType:16;
    short compType;
    struct // CompValue
    {
        short unknown;
        short valueType;
        short valueSize;
        union {
            long longValue;
            struct {
                double doubleValue;
                float floatValue;
            };
        };
    }; // compValue;

    inline double GetDoubleValue()
    {
        if (valueType == PC_LONG) {
            return (double)longValue;
        }
        else if (valueType == PC_DOUBLE) // || PC_FLOAT, same thing
        {
            return doubleValue;
        }
        return 0.0;
    };
};

inline ParamComp * GetComparisonParameter(LPRDATA rdPtr)
{
    eventParam * CurrentParam = rdPtr->rHo.hoCurrentParam;
    //	EVPNEXT(rdPtr->rHo.hoCurrentParam);
    switch (CurrentParam->evpCode) {
    case PARAM_CMPSTRING:
    case PARAM_COMPARAISON: {
        ParamComp * pData = (ParamComp *)(&(CurrentParam->evp));
        return pData;
    } break;
    }
    return NULL;
}

inline long Param_Comparison_true(
    Comparison comparison,
    long value) // Returns a value such that (return comparison value) is true
{
    switch (comparison) {
    case PC_EQUAL:
    case PC_LOWER_OR_EQUAL:
    case PC_GREATER_OR_EQUAL:
        return value;
    case PC_DIFFERENT:
    case PC_LOWER:
        return value - 1;
    case PC_GREATER:
        return value + 1;
    default:
        return 0; // ERROR!!!
    }
}

inline long Param_Comparison_false(
    Comparison comparison,
    long value) // Returns a value such that (return comparison value) is false
{
    switch (comparison) {
    case PC_DIFFERENT:
    case PC_LOWER:
    case PC_GREATER:
        return value;
    case PC_EQUAL:
    case PC_GREATER_OR_EQUAL:
        return value - 1;
    case PC_LOWER_OR_EQUAL:
        return value + 1;
    default:
        return 0; // ERROR!!!
    }
}

inline bool Param_Comparison_Test(Comparison comparison, double left, double right)
{
    switch (comparison) {
    case PC_EQUAL:
        return left == right;
    case PC_DIFFERENT:
        return left != right;
    case PC_LOWER_OR_EQUAL:
        return left <= right;
    case PC_LOWER:
        return left < right;
    case PC_GREATER_OR_EQUAL:
        return left >= right;
    case PC_GREATER:
        return left > right;
    default:
        return 0; // ERROR!!!
    }
}

inline bool Param_Comparison_Test(Comparison comparison, const char * left, const char * right)
{
    int result = strcmp(left, right);
    switch (comparison) {
    case PC_EQUAL:
        return result == 0;
    case PC_DIFFERENT:
        return result != 0;
    case PC_LOWER_OR_EQUAL:
        return result <= 0;
    case PC_LOWER:
        return result < 0;
    case PC_GREATER_OR_EQUAL:
        return result >= 0;
    case PC_GREATER:
        return result > 0;
    default:
        return 0; // ERROR!!!
    }
}

////

/*double GetDoubleParameter(LPRDATA rdPtr)
{
        eventParam * CurrentParam = rdPtr->rHo.hoCurrentParam;
        EVPNEXT(rdPtr->rHo.hoCurrentParam);
        switch (CurrentParam->evpCode)
        {
                case PARAM_EXPRESSION:
                case PARAM_COMPARAISON:
                {
                        ParamComp* pData = (ParamComp*)(&(CurrentParam->evp));
                        return pData->GetDoubleValue();
                }
                break;
        }
        return 0.0;
}*/

////

__inline rCom * getrCom(headerObject * object)
{
    DWORD OEFlags = object->hoOEFlags;
    if (!IS_SET(OEFlags, OEFLAG_MOVEMENTS) && !IS_SET(OEFlags, OEFLAG_ANIMATIONS))
        return 0;

    return (rCom *)((__int8 *)object + sizeof(headerObject));
}

__inline rMvt * getrMvt(headerObject * object)
{
    DWORD OEFlags = object->hoOEFlags;
    if (!IS_SET(OEFlags, OEFLAG_MOVEMENTS))
        return 0;

    return (rMvt *)((__int8 *)object + sizeof(headerObject) + sizeof(rCom));
}

__inline rAni * getrAni(headerObject * object)
{
    DWORD OEFlags = object->hoOEFlags;
    if (!IS_SET(OEFlags, OEFLAG_ANIMATIONS))
        return 0;

    return (rAni *)((__int8 *)object + sizeof(headerObject) + sizeof(rCom) +
                    (IS_SET(OEFlags, OEFLAG_MOVEMENTS) ? sizeof(rMvt) : 0));
}

__inline rSpr * getrSpr(headerObject * object)
{
    DWORD OEFlags = object->hoOEFlags;
    if (!IS_SET(OEFlags, OEFLAG_SPRITES))
        return 0;

    return (rSpr *)((__int8 *)object + sizeof(headerObject) +
                    ((IS_SET(OEFlags, OEFLAG_MOVEMENTS) || IS_SET(OEFlags, OEFLAG_ANIMATIONS))
                         ? sizeof(rCom)
                         : 0) +
                    (IS_SET(OEFlags, OEFLAG_MOVEMENTS) ? sizeof(rMvt) : 0) +
                    (IS_SET(OEFlags, OEFLAG_ANIMATIONS) ? sizeof(rAni) : 0));
}

__inline rVal * getrVal(headerObject * object)
{
    DWORD OEFlags = object->hoOEFlags;
    if (!IS_SET(OEFlags, OEFLAG_VALUES))
        return 0;

    return (rVal *)((__int8 *)object + sizeof(headerObject) +
                    ((IS_SET(OEFlags, OEFLAG_MOVEMENTS) || IS_SET(OEFlags, OEFLAG_ANIMATIONS))
                         ? sizeof(rCom)
                         : 0) +
                    (IS_SET(OEFlags, OEFLAG_MOVEMENTS) ? sizeof(rMvt) : 0) +
                    (IS_SET(OEFlags, OEFLAG_ANIMATIONS) ? sizeof(rAni) : 0) +
                    (IS_SET(OEFlags, OEFLAG_SPRITES) ? sizeof(rSpr) : 0));
}

#pragma pack(pop)

#else
#ifdef WARNING_MULTI_INCLUDE
#pragma message(__FILE__ " included multiple times")
#endif
#endif
