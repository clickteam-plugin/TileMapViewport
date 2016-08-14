// --------------------------------
// Condition menu
// --------------------------------

#ifdef CONDITION_MENU

SEPARATOR
SUB_START("Collisions")
ITEM(2, "Pixel is solid ?")
SEPARATOR
ITEM(0, "Object is overlapping layer ?")
ITEM(8, "Rectangle is overlapping layer ?")
SUB_START("Filters (place above overlap condition)")
SUB_START("Tile value")
ITEM(6, "X is...")
ITEM(7, "Y is...")
ITEM(9, "Value is within range...")
SUB_END
ITEM(5, "Sub-layer value is...")
SUB_END
SUB_END
SUB_START("Before rendering")
ITEM(4, "On specific layer")
ITEM(3, "On any layer")
SEPARATOR
ITEM(1, "On tile")
SUB_END
#endif

// --------------------------------
// Action menu
// --------------------------------

#ifdef ACTION_MENU

SEPARATOR
SUB_START("Display")
ITEM(0, "Set scroll position")
ITEM(35, "Set zoom")
ITEM(36, "Set zoom origin")
SEPARATOR
ITEM(2, "Set background color")
ITEM(1, "Set size")
SEPARATOR
ITEM(14, "Set layers to draw")
SUB_END
SUB_START("Animation")
ITEM(16, "Set timer")
ITEM(17, "Advance timer")
SEPARATOR
ITEM(29, "Set speed")
ITEM(30, "Set width")
ITEM(31, "Set height")
ITEM(32, "Set mode")
ITEM(33, "Set order")
SUB_END
SUB_START("Collisions")
ITEM(13, "Set collision margin")
SUB_END
SEPARATOR
SUB_START("Callbacks")
SUB_START("Layer")
ITEM(18, "Enable")
ITEM(19, "Disable")
SUB_END
SUB_START("Tile")
ITEM(4, "Enable")
ITEM(5, "Disable")
SEPARATOR
ITEM(7, "Set tile render overflow")
SUB_END
SUB_END
SUB_START("On layer")
SUB_START("Settings")
ITEM(21, "Set tileset")
SEPARATOR
ITEM(23, "Set visible")
ITEM(20, "Set opacity")
SEPARATOR
ITEM(22, "Set offset")
SEPARATOR
SUB_START("Sub-layer links")
ITEM(28, "Set animation")
SEPARATOR
ITEM(24, "To tileset")
SEPARATOR
// SUB_START("HWA only")
ITEM(27, "Set angle")
ITEM(25, "Set X scale")
ITEM(26, "Set Y scale")
// SUB_END
SUB_END
SUB_END
SUB_END
SUB_START("On tile")
ITEM(6, "Set tile value")
ITEM(15, "Set tileset")
ITEM(32, "Set animation")
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
ITEM(10, "Animation timer")
SUB_START("Callbacks")
SUB_START("On layer")
ITEM(11, "Layer index")
SUB_END
SUB_START("On tile")
ITEM(2, "X position")
ITEM(3, "Y position")
ITEM(4, "Tileset X")
ITEM(5, "Tileset Y")
SUB_END
SUB_END
SUB_START("Coordinates")
SUB_START("Viewport render position")
ITEM(0, "X")
ITEM(1, "Y")
SUB_END
SUB_START("On-screen to layer position")
ITEM(6, "X")
ITEM(7, "Y")
SUB_END
SUB_START("Layer to on-screen position")
ITEM(8, "X")
ITEM(9, "Y")
SUB_END
SUB_END
SEPARATOR

#endif