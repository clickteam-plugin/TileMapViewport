#include "Common.h"

long ProcessCondition(LPRDATA rdPtr, long param1, long param2, long (*myFunc)(LPRDATA, LPRO, long))
{
	short p1 = ((eventParam*)param1)->evp.evpW.evpW0;
	
	LPRH rhPtr = rdPtr->rHo.hoAdRunHeader;      //get a pointer to the mmf runtime header
	LPOBL objList = rhPtr->rhObjectList;     //get a pointer to the mmf object list
	LPOIL oiList = rhPtr->rhOiList;             //get a pointer to the mmf object info list
	LPQOI qualToOiList = rhPtr->rhQualToOiList; //get a pointer to the mmf qualifier to Oi list
	
	if ( p1 & 0x8000 ) // dealing with a qualifier...
	{
		LPQOI qualToOiStart = (LPQOI)(((char*)qualToOiList) + (p1 & 0x7FFF));
		LPQOI qualToOi = qualToOiStart;
		bool passed = false;
		
		for(qualToOi; qualToOi->qoiOiList >= 0; qualToOi = (LPQOI)(((char*)qualToOi) + 4))
		{
			LPOIL curOi = oiList + qualToOi->qoiOiList;
			
			if(curOi->oilNObjects <= 0) continue;	//No Objects

			bool hasSelection = curOi->oilEventCount == rhPtr->rh2.rh2EventCount;
			if(hasSelection && curOi->oilNumOfSelected <= 0) continue; //No selected objects
			
			LPHO curObj = NULL;
			LPHO prevSelected = NULL;
			int count = 0;
			int selected = 0;
			if(hasSelection) //Already has selected objects
			{
				curObj = objList[curOi->oilListSelected].oblOffset;
				count = curOi->oilNumOfSelected;
			}
			else //No previously selected objects
			{
				curObj = objList[curOi->oilObject].oblOffset;
				count = curOi->oilNObjects;
				curOi->oilEventCount = rhPtr->rh2.rh2EventCount; //tell mmf that the object selection is relevant to this event
			}
			
			for(int i = 0; i < count; i++)
			{
				//Check here
				if(myFunc(rdPtr,(LPRO)curObj,param2))
				{
					if(selected++ == 0)
					{
						curOi->oilListSelected = curObj->hoNumber;
					}
					else
					{
						prevSelected->hoNextSelected = curObj->hoNumber;
					}
					prevSelected = curObj;
				}
				if(hasSelection)
				{
					if(curObj->hoNextSelected >= 0) curObj = objList[curObj->hoNextSelected].oblOffset;
					else break;
				}
				else
				{
					if(curObj->hoNumNext >= 0) curObj = objList[curObj->hoNumNext].oblOffset;
					else break;
				}
			}
			curOi->oilNumOfSelected = selected;
			if ( selected > 0 )
			{
				prevSelected->hoNextSelected = -1;
				passed = true;
			}
			else
			{
				curOi->oilListSelected = -32768;
			}
		}
		
		return passed;
	}
	else	// Not a qualifier
	{
		LPOIL curOi = oiList + p1;
		if(curOi->oilNObjects <= 0) return false;	//No Objects

		bool hasSelection = curOi->oilEventCount == rhPtr->rh2.rh2EventCount;
		if(hasSelection && curOi->oilNumOfSelected <= 0) return false; //No selected objects
		
		LPHO curObj = NULL;
		LPHO prevSelected = NULL;
		int count = 0;
		int selected = 0;
		if(hasSelection) //Already has selected objects
		{
			curObj = objList[curOi->oilListSelected].oblOffset;
			count = curOi->oilNumOfSelected;
		}
		else //No previously selected objects
		{
			curObj = objList[curOi->oilObject].oblOffset;
			count = curOi->oilNObjects;
			curOi->oilEventCount = rhPtr->rh2.rh2EventCount; //tell mmf that the object selection is relevant to this event
		}

		for(int i = 0; i < count; i++)
		{
			//Check here
			if(myFunc(rdPtr,(LPRO)curObj,param2))
			{
				if(selected++ == 0)
				{
					curOi->oilListSelected = curObj->hoNumber;
				}
				else
				{
					prevSelected->hoNextSelected = curObj->hoNumber;
				}
				prevSelected = curObj;
			}
			if(hasSelection)
			{
				if(curObj->hoNextSelected < 0) break;
				else curObj = objList[curObj->hoNextSelected].oblOffset;
			}
			else
			{
				if(curObj->hoNumNext < 0) break;
				else curObj = objList[curObj->hoNumNext].oblOffset;
			}
		}
		curOi->oilNumOfSelected = selected;
		if ( selected > 0 )
		{
			prevSelected->hoNextSelected = -1;
			return true;
		}
		else
		{
			curOi->oilListSelected = -32768;
		}
		return false;
	}
}