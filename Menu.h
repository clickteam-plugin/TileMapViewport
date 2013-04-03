// --------------------------------
// Condition menu
// --------------------------------

#ifdef CONDITION_MENU

	SEPARATOR
	ITEM(0,"Object is overlapping layer ?")
	SEPARATOR
	ITEM(1,"On tile (before rendering)")
#endif

// --------------------------------
// Action menu
// --------------------------------

#ifdef ACTION_MENU
	
	SEPARATOR
	SUB_START("Display")
		ITEM(0, "Set scroll position")
		SEPARATOR
		ITEM(2, "Set background color")
		ITEM(1, "Set size")
	SUB_END
	SEPARATOR
	SUB_START("Callbacks")
		ITEM(4, "Enable")
		ITEM(5, "Disable")
		SEPARATOR
		ITEM(7, "Set tile render overflow")
		SEPARATOR
	SUB_END
	SUB_START("On tile")
		ITEM(3, "Set offset")
		ITEM(6, "Set tile")
		SEPARATOR
		SUB_START("HWA only")
			ITEM(8, "Set angle")
			ITEM(9, "Set X scale")
			ITEM(10, "Set Y scale")
		SUB_END
	SUB_END
	SEPARATOR

#endif


// --------------------------------
// Expression menu
// --------------------------------

#ifdef EXPRESSION_MENU

	SEPARATOR
	SUB_START("Callbacks")
		ITEM(2, "X position")
		ITEM(3, "Y position")
		ITEM(4, "Tileset X")
		ITEM(5, "Tileset Y")
	SUB_END
	SEPARATOR
	SUB_START("Layer")
		SUB_START("On-screen position")
			ITEM(0, "X")
			ITEM(1, "Y")
		SUB_END
	SUB_END
	SEPARATOR


#endif