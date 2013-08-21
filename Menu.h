// --------------------------------
// Condition menu
// --------------------------------

#ifdef CONDITION_MENU

	SEPARATOR
	ITEM(0, "Object is overlapping layer ?")
	ITEM(2, "Pixel is solid ?")
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
		SEPARATOR
		ITEM(14, "Set layers to draw")
	SUB_END
	SUB_START("Collisions")
		ITEM(13, "Set collision margin")
	SUB_END
	SEPARATOR
	SUB_START("Callbacks")
		ITEM(4, "Enable")
		ITEM(5, "Disable")
		SEPARATOR
		ITEM(7, "Set tile render overflow")
	SUB_END
	SUB_START("On tile")
		ITEM(6, "Set tile")
		ITEM(15, "Set tileset")
		SEPARATOR
		ITEM(3, "Set offset")
		ITEM(11, "Set opacity")
		SEPARATOR
		SUB_START("HWA only")
			ITEM(12, "Set tint")
			SEPARATOR
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
		SUB_START("Viewport render position")
			ITEM(0, "X")
			ITEM(1, "Y")
		SUB_END
		SUB_START("On-screen to layer position")
			ITEM(6, "X")
			ITEM(7," Y")
		SUB_END
		SUB_START("Layer to on-screen position")
			ITEM(8, "X")
			ITEM(9, "Y")
		SUB_END
	SUB_END
	SEPARATOR


#endif