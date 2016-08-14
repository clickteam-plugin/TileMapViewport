#include "ObjectSelection.h"

object * GetSingleInstace(LPRDATA rdPtr, short Oi)
{
    LPRH rhPtr =
        rdPtr->rHo.hoAdRunHeader; // get a pointer to the mmf runtime header
    LPOBL ObjectList =
        rhPtr->rhObjectList;        // get a pointer to the mmf object list
    LPOIL OiList = rhPtr->rhOiList; // get a pointer to the mmf object info list
    LPQOI QualToOiList =
        rhPtr->rhQualToOiList; // get a pointer to the mmf qualifier to Oi list

    // Read second object parameter - load first instance, ignore rest
    LPOIL CurrentOi = NULL;
    if (Oi & 0x8000) {
        LPQOI CurrentQualToOi = PtrAddBytes(QualToOiList, Oi & 0x7FFF);
        CurrentOi = GetOILPtr(OiList, CurrentQualToOi->qoiOiList);
    }
    else {
        CurrentOi = GetOILPtr(OiList, Oi);
    }
    if (CurrentOi) {
        bool prevSelected = (CurrentOi->oilEventCount == rhPtr->rh2.rh2EventCount); // find out if conditions have selected any objects yet
        if (prevSelected &&
            CurrentOi->oilNumOfSelected <=
                0) // if "0" have been selected (blame qualifiers) then return
            return NULL;

        object * CurrentObject = NULL;
        object * PrevSelected = NULL;
        int iCount = 0;
        int numSelected = 0;
        if (prevSelected) {
            CurrentObject = ObjectList[CurrentOi->oilListSelected].oblOffset;
            iCount = CurrentOi->oilNumOfSelected;
        }
        else {
            CurrentObject = ObjectList[CurrentOi->oilObject].oblOffset;
        }

        return CurrentObject;
    }

    return NULL;
}

// Run a custom filter on the SOL (via function callback)
bool FilterObjects(LPRDATA rdPtr, short Oi, bool (*filterFunction)(LPRDATA, LPRO, void *),
                   void * userdata, bool is_negated)
{

    LPRH rhPtr =
        rdPtr->rHo.hoAdRunHeader; // get a pointer to the mmf runtime header
    LPOBL ObjectList =
        rhPtr->rhObjectList;        // get a pointer to the mmf object list
    LPOIL OiList = rhPtr->rhOiList; // get a pointer to the mmf object info list
    LPQOI QualToOiList =
        rhPtr->rhQualToOiList; // get a pointer to the mmf qualifier to Oi list

    bool bSelected = false;

    if (Oi & 0x8000) // dealing with a qualifier...
    {
        // For qualifiers evpW0( & 0x7FFF ) is the offset in the qualToOi array
        // And evpW1( & 0x7FFF ) is the qualifier number.
        LPQOI CurrentQualToOiStart = PtrAddBytes(QualToOiList, Oi & 0x7FFF);
        LPQOI CurrentQualToOi = CurrentQualToOiStart;

        // Loop through all objects associated with this qualifier
        for (CurrentQualToOi; CurrentQualToOi->qoiOiList >= 0;
             CurrentQualToOi = PtrAddBytes(CurrentQualToOi, 4)) {
            LPOIL CurrentOi = GetOILPtr(OiList, CurrentQualToOi->qoiOiList); // get a pointer to the
            // objectInfo for this
            // object in the qualifier
            if (CurrentOi->oilNObjects <=
                0) // skip if there are none of the object
                continue;

            bool prevSelected = (CurrentOi->oilEventCount == rhPtr->rh2.rh2EventCount); // find out if conditions have
            // selected any objects yet
            if (prevSelected &&
                CurrentOi->oilNumOfSelected <=
                    0) // if "0" have been selected (blame qualifiers) then skip
                continue;

            object * CurrentObject = NULL;
            object * PrevSelected = NULL;
            int iCount = 0;
            int numSelected = 0;
            if (prevSelected) {
                CurrentObject = ObjectList[CurrentOi->oilListSelected].oblOffset;
                iCount = CurrentOi->oilNumOfSelected;
            }
            else {
                CurrentObject = ObjectList[CurrentOi->oilObject].oblOffset;
                iCount = CurrentOi->oilNObjects;
                CurrentOi->oilEventCount =
                    rhPtr->rh2.rh2EventCount; // tell mmf that the object
                                              // selection is relevant to this
                                              // event
            }
            for (int i = 0; i < iCount; i++) {
                if (is_negated ^ filterFunction(rdPtr, (LPRO)CurrentObject, userdata)) {
                    if (numSelected++ == 0) {
                        CurrentOi->oilListSelected = CurrentObject->hoNumber; // select the first object
                    }
                    else {
                        PrevSelected->hoNextSelected = CurrentObject->hoNumber;
                    }

                    PrevSelected = CurrentObject;
                }

                if (prevSelected) {
                    if (CurrentObject->hoNextSelected >= 0) {
                        CurrentObject =
                            ObjectList[CurrentObject->hoNextSelected].oblOffset;
                    }
                    else {
                        break;
                    }
                }
                else {
                    if (CurrentObject->hoNumNext >= 0) {
                        CurrentObject = ObjectList[CurrentObject->hoNumNext].oblOffset;
                    }
                    else {
                        break;
                    }
                }
            }

            CurrentOi->oilNumOfSelected = numSelected;
            if (numSelected > 0) {
                PrevSelected->hoNextSelected = -1;
                bSelected = true;
            }
            else {
                CurrentOi->oilListSelected = -1;
            }
        }

        return bSelected ^ is_negated;
    }
    else {
        LPOIL CurrentOi = GetOILPtr(
            OiList, Oi); // get a pointer to the objectInfo for this object
        if (CurrentOi->oilNObjects <=
            0) // return if there are none of the object
            return false ^ is_negated;

        bool prevSelected = (CurrentOi->oilEventCount == rhPtr->rh2.rh2EventCount); // find out if conditions have selected
        // any objects yet
        if (prevSelected &&
            CurrentOi->oilNumOfSelected <=
                0) // if "0" have been selected (blame qualifiers) then return
            return false ^ is_negated;

        object * CurrentObject = NULL;
        object * PrevSelected = NULL;
        int iCount = 0;
        int numSelected = 0;
        if (prevSelected) {
            CurrentObject = ObjectList[CurrentOi->oilListSelected].oblOffset;
            iCount = CurrentOi->oilNumOfSelected;
        }
        else {
            CurrentObject = ObjectList[CurrentOi->oilObject].oblOffset;
            iCount = CurrentOi->oilNObjects;
            CurrentOi->oilEventCount =
                rhPtr->rh2.rh2EventCount; // tell mmf that the object selection
                                          // is relevant to this event
        }
        for (int i = 0; i < iCount; i++) {
            if (is_negated ^ filterFunction(rdPtr, (LPRO)CurrentObject, userdata)) {
                if (numSelected++ == 0) {
                    CurrentOi->oilListSelected =
                        CurrentObject->hoNumber; // select the first object
                }
                else {
                    PrevSelected->hoNextSelected = CurrentObject->hoNumber;
                }

                PrevSelected = CurrentObject;
            }

            if (prevSelected) {
                if (CurrentObject->hoNextSelected < 0) {
                    break;
                }
                else {
                    CurrentObject = ObjectList[CurrentObject->hoNextSelected].oblOffset;
                }
            }
            else {
                if (CurrentObject->hoNumNext < 0) {
                    break;
                }
                else {
                    CurrentObject = ObjectList[CurrentObject->hoNumNext].oblOffset;
                }
            }
        }

        CurrentOi->oilNumOfSelected = numSelected;
        if (numSelected > 0) {
            PrevSelected->hoNextSelected = -1;
        }

        return (numSelected > 0) ^ is_negated;
    }
}