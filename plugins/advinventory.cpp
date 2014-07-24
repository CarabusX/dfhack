#include <string>
#include <vector>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>

#include "Console.h"
#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "MiscUtils.h"
#include "PluginManager.h"
#include <modules/Gui.h>
#include <modules/Screen.h>
#include <modules/Items.h>

#include "df/world.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/ui_advmode.h"
#include "df/interface_key.h"
#include "df/nemesis_record.h"
#include "df/unit.h"
#include "df/caste_body_info.h"
#include "df/unit_inventory_item.h"
#include "df/item.h"
#include "df/item_helmst.h"
#include "df/item_pantsst.h"
#include "df/item_armorst.h"
#include "df/item_glovesst.h"
#include "df/item_shoesst.h"
#include "df/itemdef_helmst.h"
#include "df/itemdef_pantsst.h"
#include "df/itemdef_armorst.h"
#include "df/itemdef_glovesst.h"
#include "df/itemdef_shoesst.h"
#include "df/general_ref_contains_itemst.h"

using std::string;
using std::vector;
using std::set;

using namespace DFHack;
using namespace df::enums;

using df::global::gps;
using df::global::ui_advmode;
using df::global::world;


color_ostream* out;

inline void Capitalize (string& str)
{
	if (!str.empty())
	{
		str[0] = toupper(str[0]);
	}
}

inline bool OutputTile (int x, int y, char ch, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK)
{
	return Screen::paintTile ( Screen::Pen(ch, color, bg_color), x, y);
}

bool OutputString (int& x, int y, const string& text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK, bool rightAligned = false)
{
	int leftX = x;
	if (rightAligned) {
		leftX -= (text.length() - 1);
	} else {
		x += text.length();
	}
    return Screen::paintString( Screen::Pen(' ', color, bg_color), leftX, y, text ); //
}

inline bool OutputString (const int& x, int y, const string& text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK, bool rightAligned = false)
{
	int _x = x;
	return OutputString(_x, y, text, color, bg_color, rightAligned);
}

bool OutputLimitedString (int x, int y, const string& text, int maxLength, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK)
{
	if (maxLength < text.length()) {
		return OutputString(x, y, text.substr (0, maxLength), color, bg_color);
	} else {
		return OutputString(x, y, text, color, bg_color);
	}
}

void OutputHotkeyString (int& x, int y, const char* text, const string& hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE, bool addSemicolon = true)
{
    OutputString(x, y, hotkey, hotkey_color);
    string display( addSemicolon ? ": " : " " );
    display.append(text);
    OutputString(x, y, display, text_color);
}

void OutputHotkeyString (const int& x, int y, const char* text, const string& hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE, bool addSemicolon = true)
{
	int _x = x;
	OutputHotkeyString(_x, y, text, hotkey, hotkey_color, text_color, addSemicolon);
}


//Gui::getSelectedUnit(*out, true)
df::unit* getPlayerUnit()
{
    df::nemesis_record* playerNemesis = vector_get(world->nemesis.all, ui_advmode->player_id);

    if (!playerNemesis)
        return nullptr;

	return playerNemesis->unit;
}


static void addQualityChars (std::string& str, int16_t quality)
{
	static const char QUALITY_CHARS[] = { 0, '-', '+', '*', '\xF0', '\x0F' };

    if (quality > 0 && quality <= 5) {
        char c = QUALITY_CHARS[quality];
        str = c + str + c;
    }
}

string getItemFullName (df::item *item)
{
	static const char DECORATION_LEFT  = '\xAE';
	static const char DECORATION_RIGHT = '\xAF';

    string str;
    item->getItemDescription(&str, 0);

    addQualityChars(str, item->getQuality());

    if (item->isImproved()) {
        str = DECORATION_LEFT + str + DECORATION_RIGHT;
        addQualityChars(str, item->getImprovementQuality());
    }

    return str;
}


df::armor_properties* getArmorProperties (df::item *item) {
	df::armor_properties* props = nullptr;

	using namespace df::enums::item_type;

	switch (item->getType()) {
		case HELM:
		{
			auto armorItem = virtual_cast<df::item_helmst>(item);
			if (armorItem)
				props = &armorItem->subtype->props;
			break;
		}
		case ARMOR:
		{
			//auto armorItem = (df::item_armorst*)item;
			auto armorItem = virtual_cast<df::item_armorst>(item);
			if (armorItem)
				props = &armorItem->subtype->props;
			break;
		}
		case PANTS:
		{
			auto armorItem = virtual_cast<df::item_pantsst>(item);
			if (armorItem)
				props = &armorItem->subtype->props;
			break;
		}
		case GLOVES:
		{
			auto armorItem = virtual_cast<df::item_glovesst>(item);
			if (armorItem)
				props = &armorItem->subtype->props;
			break;
		}
		case SHOES:
		{
			auto armorItem = virtual_cast<df::item_shoesst>(item);
			if (armorItem)
				props = &armorItem->subtype->props;
			break;
		}
		default:
			break;
	}

	return props;
}


enum PossibleActionFlags {
	POSSIBLE_EAT,
	POSSIBLE_INTERACT,
	POSSIBLE_WEAR,
	POSSIBLE_WEARABLE,
	NUM_POSSIBLE_ACTION_FLAGS,
	NUM_DISPLAYED_ACTION_FLAGS = 3
};


struct SortableItem;
typedef vector< SortableItem* const > SortableItemsVector;

struct SortableItem {
	static const vector< df::body_part_raw* >* playerBodyParts;

	const df::unit_inventory_item *const inv_item;
	df::item *const item;
	string fullName;
	string bodyPartName;

	int8_t color;
	int8_t indent;
	int totalChildren;
	//bool collapsed;
	bool possibleActions [NUM_POSSIBLE_ACTION_FLAGS];

	uint32_t sortingKey;
	SortableItemsVector* containedItems;
	SortableItemsVector* containedItemsSorted;

	//inline const SortableItemsVector* getContainedItems() const;
	inline void addToAllItems();
	inline void addToAllItemsSorted();


	SortableItem (const df::unit_inventory_item* _inv_item)
		: inv_item(_inv_item), item(_inv_item->item), indent(0), totalChildren(0)/*, collapsed(false)*/
	{
		init();

		calculateKey_Mode_BodyPart();
	}

	SortableItem (df::item* _item, int8_t _indent)
		: inv_item(nullptr), item(_item), indent(_indent), totalChildren(0)/*, collapsed(false)*/
	{
		init();

		color = COLOR_LIGHTGREEN;
	}

	void init()
	{
		std::fill_n( possibleActions, (int)NUM_POSSIBLE_ACTION_FLAGS, false );
		sortingKey = 0;
		calculateKey_ItemType();

		addToAllItems();
		initContainer();

		fullName = getItemFullName(item);

		if (totalChildren > 0)
		{
			std::stringstream ss;
			ss << fullName << "  <" << totalChildren << ">";
			fullName = ss.str();
		}
	}

	~SortableItem()
	{
		if (containedItems) {
			for (auto it = containedItems->cbegin(); it != containedItems->cend(); ++it)
			{
				delete *it;
			}

			delete containedItems;
			delete containedItemsSorted;
		}
	}

	void initContainer()
	{
		SortableItemsVector*& tempVector = createTempVector();
		containedItems = tempVector;
		tempVector     = nullptr;

		int8_t newIndent = indent + 2;

		auto& refs = item->general_refs;
		//for (auto& ref : refs)
		for (auto it = refs.cbegin(); it != refs.cend(); ++it)
		{
			df::general_ref *ref = *it;

			//if (!strict_virtual_cast<df::general_ref_contains_itemst>(ref))
			if (ref->getType() != general_ref_type::CONTAINS_ITEM)
				continue;

			df::item *childItem = ref->getItem();
			if (!childItem) continue;

			SortableItem* childSortableItem = new SortableItem(childItem, newIndent);
			containedItems->push_back( childSortableItem );
			totalChildren += childSortableItem->totalChildren;
		}

		if (!containedItems->empty()) {
			//indent = newIndent;
			totalChildren += containedItems->size();

			containedItemsSorted = new SortableItemsVector (*containedItems);
			std::sort (containedItemsSorted->begin(), containedItemsSorted->end(), sortingFunction);
		}
		else
		{
			if (!tempVector) {
				tempVector = containedItems; // return vector for reuse
			} else {
				delete containedItems;
			}

			containedItems       = nullptr;
			containedItemsSorted = nullptr;
		}
	}

	static SortableItemsVector*& createTempVector()
	{
		static SortableItemsVector* tempVector = nullptr;
		if (!tempVector)
		{
			tempVector = new SortableItemsVector();
		}
		return tempVector;
	}


	static bool sortingFunction (const SortableItem* a, const SortableItem* b)
	{
		return (a->sortingKey < b->sortingKey);
	}

	// sortingKey consists of:
	//  2 bits - inventory mode
	//  4 bits - item type
	// 16 bits - body part id
	//  2 bits - layer
	//  8 bits - layer permit
	void calculateKey_Mode_BodyPart() {
		int16_t body_part_id = inv_item->body_part_id;
		bodyPartName = getBodyPartName (body_part_id);
		Capitalize (bodyPartName);

		uint32_t key;

		using df::unit_inventory_item;

		switch (inv_item->mode)
		{
			case unit_inventory_item::Weapon:
				color = COLOR_GREY;
				key = 0;
				break;
			case unit_inventory_item::Piercing:
				bodyPartName = "In " + bodyPartName;
				// continue
			case unit_inventory_item::Worn:
			case unit_inventory_item::Flask:
				color = COLOR_LIGHTCYAN;
				key = 1;
				break;
			case unit_inventory_item::StuckIn:
				bodyPartName = "Stuck in " + bodyPartName;
				color = COLOR_LIGHTRED;
				key = 2;
				break;
			default:
				color = COLOR_WHITE;
				key = 3;
		}

		sortingKey |= (key << (4 + 16 + 2 + 8)); //30

		key = (uint32_t)( body_part_id );
		sortingKey |= (key << (2 + 8)); //10
	}

	const string& getBodyPartName (int16_t body_part_id)
	{
		df::body_part_raw* bodyPart = (*playerBodyParts) [ body_part_id ];
		return *(bodyPart->name_singular[0]);
	}

	void calculateKey_ItemType() {
		uint32_t key = getKey_ItemType(item);
		sortingKey |= (key << (16 + 2 + 8)); //26

		calculateKey_Layer(); //0
	}

	static uint32_t getKey_ItemType (df::item *item)
	{
		using namespace df::enums::item_type;

		switch (item->getType()) {
			case WEAPON:
				return 0;
			case SHIELD:
				return 1;
			case HELM:
				return 2;
			case ARMOR:
				return 3;
			case PANTS:
				return 4;
			case GLOVES:
				return 5;
			case SHOES:
				return 6;
			case AMMO:
				return 7;
			case COIN:
				return 8;
			default:
				return 9;  // other items
			case FLASK:
				return 10;
			case QUIVER:
				return 11;
			case BACKPACK:
				return 12;
			case BOX:
			case BIN:
				return 13;  //containers go at the end
		}
	}


	void calculateKey_Layer() {
		df::armor_properties* props = getArmorProperties(item);

		if (!props)
			return;

		uint32_t key;
		key = (uint32_t)( props->layer );
		sortingKey |= (key << (8)); //8

		key = (uint32_t)( props->layer_permit );
		key = std::min( key, 255U );
		sortingKey |= (key << (0));
	}
};

const vector< df::body_part_raw* >* SortableItem::playerBodyParts = nullptr;


struct Inventory {
	const df::unit* playerUnit;

	SortableItemsVector items;  // doesn't include items in containers
	SortableItemsVector itemsSorted;
	SortableItemsVector allItems;  // includes items in containers
	SortableItemsVector allItemsSorted;

	//inline const SortableItemsVector& getItems() const;
	inline const SortableItemsVector& getAllItems() const;

	int firstVisibleIndex;


	Inventory()
		: firstVisibleIndex(0)
	{
		playerUnit = getPlayerUnit();

        SortableItem::playerBodyParts = (playerUnit ? &playerUnit->body.body_plan->body_parts : nullptr);
	}

	void loadItems()
	{
		if (!playerUnit)
			return;

		auto &unitInventory = playerUnit->inventory;

		for (auto it = unitInventory.cbegin(); it != unitInventory.cend(); ++it)
		{
			const df::unit_inventory_item* inv_item = *it;
			items.push_back( new SortableItem(inv_item) );
		}

		itemsSorted = items;
		std::sort (itemsSorted.begin(), itemsSorted.end(), SortableItem::sortingFunction);

		for (auto it = itemsSorted.cbegin(); it != itemsSorted.cend(); ++it)
		{
			(*it)->addToAllItemsSorted();
		}
	}

	~Inventory()
	{
		for (auto it = items.cbegin(); it != items.cend(); ++it)
		{
			delete *it;
		}
	}
};


enum InventoryAction {
    ACTION_DROP,
    ACTION_EAT,
    ACTION_GET,
    ACTION_INTERACT,
    ACTION_PUT,
    ACTION_REMOVE,
    ACTION_THROW,
    ACTION_VIEW,
    ACTION_WEAR,
	NUM_ACTIONS
};


struct InventoryActionInfo {
	const df::interface_key key;
	const string label;
	const bool allowMultiSelect;
};

InventoryActionInfo inventoryActions [NUM_ACTIONS] = {
	{ interface_key::CUSTOM_SHIFT_D, "rop"    , true  },
	{ interface_key::CUSTOM_SHIFT_E, "at"     , false }, // "at or drink"
	{ interface_key::CUSTOM_SHIFT_G, "et"     , true  },
	{ interface_key::CUSTOM_SHIFT_I, "nteract", false },
	{ interface_key::CUSTOM_SHIFT_P, "ut"     , true  },
	{ interface_key::CUSTOM_SHIFT_R, "emove"  , true  },
	{ interface_key::CUSTOM_SHIFT_T, "hrow"   , true  },
	{ interface_key::CUSTOM_SHIFT_V, "iew"    , false },
	{ interface_key::CUSTOM_SHIFT_W, "ear"    , false }
};

const df::interface_key A_INV_ADVINVENTORY = interface_key::CHANGETAB;


class ViewscreenAdvInventory : public dfhack_viewscreen
{
public:
	static const int itemsListTopY     =  2;
	static const int itemsListBottomY_ = -7;
	static const int itemsListLeftX    =  0;
	static const int itemsListRightX_  = -1;

	static bool isActive;
	static bool sortItems;
	static InventoryAction currentAction;
	static Inventory* inventory;


	static void show()
	{
		/*if (!instance)
			instance = new ViewscreenAdvInventory();
		else
			instance->parent = nullptr;
		Screen::show(instance);*/

		ViewscreenAdvInventory* screen = new ViewscreenAdvInventory();

		if (!inventory)
		{
			inventory = new Inventory();
			inventory->loadItems();
		}

		Screen::show(screen);
	}

	static void onLeaveScreen()
	{
		if (inventory)
		{
			delete inventory;
			inventory = nullptr;
		}
	}

    void feed(set<df::interface_key> *input)
    {
        if (input->count(interface_key::LEAVESCREEN))
        {
			input->clear();
            Screen::dismiss(this);

			input->insert(interface_key::LEAVESCREEN);
			parent->feed(input);
            return;
        }
		else if (input->count(A_INV_ADVINVENTORY))
        {
			isActive = false;
			input->clear();
            Screen::dismiss(this);

			//ui_advmode->menu = ui_advmode_menu::Inventory;
            
			/*if (VIRTUAL_CAST_VAR(unitlist, df::viewscreen_unitlistst, parent))
			{
				if (events->count(interface_key::UNITJOB_VIEW) || events->count(interface_key::UNITJOB_ZOOM_CRE))
				{
					for (int i = 0; i < unitlist->units[unitlist->page].size(); i++)
					{
						if (unitlist->units[unitlist->page][i] == units[input_row]->unit)
						{
							unitlist->cursor_pos[unitlist->page] = i;
							unitlist->feed(events);
							if (Screen::isDismissed(unitlist))
								Screen::dismiss(this);
							else
								do_refresh_names = true;
							break;
						}
					}
				}
			}*/

            return;
        }
		else if (input->count(interface_key::CUSTOM_SHIFT_S))
        {
			sortItems = !sortItems;
        }
		else
		{
			for (int i = 0; i < NUM_ACTIONS; i++)
			{
				if (input->count( inventoryActions[i].key ))
				{
					if (i != currentAction)
					{
						currentAction = (InventoryAction)i;
					}
					return;
				}
			}
		}
    }

    void render()
    {
        if (Screen::isDismissed(this))
            return;

        dfhack_viewscreen::render(); //calls check_resize();

        Screen::clear();
        //Screen::drawBorder("  Advanced Inventory  ");

		renderItemsList();

		int x, y;
		auto dim = Screen::getWindowSize();
		OutputString(0, 0, "Advanced Inventory");

		OutputString(dim.x - 2, 0, "DFHack", COLOR_LIGHTMAGENTA, COLOR_BLACK, true);
		OutputString(dim.x - 2, dim.y - 1, "Written by Carabus/Rafal99", COLOR_DARKGREY, COLOR_BLACK, true);

		x = OutputHotkeysLine1(dim.y - 1);
		OutputHotkeyString(x, dim.y - 1, "Basic Inventory   ", Screen::getKeyDisplay(A_INV_ADVINVENTORY), COLOR_LIGHTRED); //gps->dimy - 2

		x = 0; y = dim.y - 2;
		OutputString(x, y, "Actions:", COLOR_WHITE);
		x += 3;

		for (int i = 0; i < NUM_ACTIONS; i++) {
			OutputString(x, y, Screen::getKeyDisplay( inventoryActions[i].key ), COLOR_LIGHTGREEN);
			OutputString(x, y, inventoryActions[i].label, (i == currentAction) ? ((currentAction == ACTION_VIEW) ? COLOR_LIGHTCYAN : COLOR_RED) : COLOR_GREY);
			x += 3;
		}

		x = 0; y = dim.y - 4;
		/*OutputHotkeyString(x, y, "Expand/Collapse", Screen::getKeyDisplay(interface_key::CUSTOM_SHIFT_C), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 3;
		OutputHotkeyString(x, y, "Expand/Collapse All", Screen::getKeyDisplay(interface_key::CUSTOM_CTRL_C), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 3;*/
		OutputHotkeyString(x, y, sortItems ? "Items are sorted" : "Items are not sorted", Screen::getKeyDisplay(interface_key::CUSTOM_SHIFT_S), COLOR_LIGHTGREEN, sortItems ? COLOR_WHITE : COLOR_GREY);
		//"Sort items" : "Do not sort items"

		OutputString(79, dim.y - 5, "|");
	}

	void renderItemsList() {
		auto dim = Screen::getWindowSize();
		const int itemsListRightX  = dim.x + itemsListRightX_;
		const int itemsListBottomY = dim.y + itemsListBottomY_;

		const int hotkeyX      = itemsListLeftX;
		const int selectedX    = hotkeyX   + 2;
		const int itemNameX    = selectedX + 2;
		const int scrollBarX   = itemsListRightX;
		const int actionFlagsX = scrollBarX - 1 - NUM_DISPLAYED_ACTION_FLAGS;
		const int weightX      = actionFlagsX - 1 - 5;
		const int bodyPartX    = std::min( weightX - 1 - 24, 50 ); //25

		const SortableItemsVector& allItems = inventory->getAllItems();
		const int first    = inventory->firstVisibleIndex;
		const int maxRows  = itemsListBottomY + 1 - itemsListTopY;
		const int itemRows = std::min( (int)allItems.size() - first, maxRows );

		const int numHotkeyLetters = std::min( itemRows, 26 );
		const int numHotkeyDigits  = std::min( itemRows - 26, 10 );

		static Screen::Pen hotkeyPen (0, COLOR_LIGHTGREEN, COLOR_BLACK);
		for (int i = 0; i < numHotkeyLetters; i++)
		{
			Screen::paintTile (hotkeyPen.chtile(char('a' + i)), hotkeyX, itemsListTopY + i);
		}
		if (numHotkeyDigits > 0)
		{
			for (int i = 0; i < numHotkeyDigits; i++)
			{
				Screen::paintTile (hotkeyPen.chtile(char('0' + i)), hotkeyX, itemsListTopY + 26 + i);
			}
		}

		//static const Screen::Pen dotPen ('.', COLOR_LIGHTGREEN, COLOR_BLACK);
		static const Screen::Pen dotPen     ('\xC3', COLOR_GREEN, COLOR_BLACK);
		static const Screen::Pen dotPenLast ('\xC0', COLOR_GREEN, COLOR_BLACK);
		for (int i = 0; i < itemRows; i++)
		{
			SortableItem* sItem = allItems[first + i];
			const int x = itemNameX + sItem->indent;
			const int y = itemsListTopY + i;

			OutputTile   (selectedX, y, '-', sItem->color);
			OutputString (x, y, sItem->fullName, sItem->color);
			OutputString (bodyPartX, y, sItem->bodyPartName, sItem->color);

			if (sItem->totalChildren > 0)
			{
				int yLast = y + sItem->totalChildren;
				if (sItem->totalChildren > 1)
				{
					int y2 = std::min( yLast - 1, itemsListBottomY );
					Screen::fillRect(dotPen, x, y + 1, x, y2);
				}
				if (yLast <= itemsListBottomY)
				{
					Screen::paintTile (dotPenLast, x, yLast);
				}
			}
		}

		std::stringstream ss;
		ss << "Total items: " << allItems.size() << " ";
		OutputString(0, dim.y - 5, ss.str(), COLOR_GREY);
	}


	string getFocusString() { return "adv_inventory"; }
    //void onShow() {};

private:
	ViewscreenAdvInventory()
	{
	}

public:
	static int getTabHotkeyStringX() {
		if (tabHotkeyStringX == -1)
		{
			tabHotkeyStringX = OutputHotkeysLine1(-1);
		}
		return tabHotkeyStringX;
	}

private:
	static int tabHotkeyStringX;

	static int OutputHotkeysLine1 (int y)
	{
		int x = 0;
		OutputHotkeyString(x, y, "to view other pages.",
			Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEUP) + Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEDOWN),
			COLOR_LIGHTGREEN, COLOR_GREY, false);
		x += 2;
		OutputHotkeyString(x, y, "when done.",
			Screen::getKeyDisplay(interface_key::LEAVESCREEN),
			COLOR_LIGHTGREEN, COLOR_GREY, false);
		x += 3;
		return x;
	}
};

bool ViewscreenAdvInventory::isActive  = false;
bool ViewscreenAdvInventory::sortItems = false; //true;
InventoryAction ViewscreenAdvInventory::currentAction = ACTION_VIEW;
Inventory* ViewscreenAdvInventory::inventory = nullptr;
int ViewscreenAdvInventory::tabHotkeyStringX = -1;


/*inline const SortableItemsVector* SortableItem::getContainedItems() const
{
	return (ViewscreenAdvInventory::sortItems ? containedItemsSorted : containedItems);
}

inline const SortableItemsVector& Inventory::getItems() const
{
	return (ViewscreenAdvInventory::sortItems ? itemsSorted : items);
}*/

inline const SortableItemsVector& Inventory::getAllItems() const
{
	return (ViewscreenAdvInventory::sortItems ? allItemsSorted : allItems);
}

inline void SortableItem::addToAllItems() {
	ViewscreenAdvInventory::inventory->allItems.push_back(this);
}

inline void SortableItem::addToAllItemsSorted() {
	SortableItemsVector& allItemsSorted = ViewscreenAdvInventory::inventory->allItemsSorted;

	allItemsSorted.push_back(this);

	if (containedItemsSorted) {
		for (auto it = containedItemsSorted->cbegin(); it != containedItemsSorted->cend(); ++it)
		{
			SortableItem* sItem = *it;
			sItem->addToAllItemsSorted();
		}
	}
}


DFHACK_PLUGIN("advinventory");

DFHACK_PLUGIN_IS_ENABLED(is_enabled);

struct advinventory_hook : df::viewscreen_dungeonmodest {
    typedef df::viewscreen_dungeonmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, feed, (set<df::interface_key> *input))
    {
        switch (ui_advmode->menu)
        {
			case ui_advmode_menu::Default:
				if (input->count(interface_key::CUSTOM_CTRL_I))
				{
					ViewscreenAdvInventory::isActive = true;
					input->clear();
					input->insert(interface_key::A_INV_LOOK);
				}
				break;
			case ui_advmode_menu::Inventory:
				if (input->count(A_INV_ADVINVENTORY))
				{
					ViewscreenAdvInventory::isActive = true;
					ViewscreenAdvInventory::show();
					return;
				}
				else if (input->count(interface_key::LEAVESCREEN))
				{
					ViewscreenAdvInventory::onLeaveScreen();
				}
				break;
			default:
				break;
        }

        INTERPOSE_NEXT(feed)(input);
    }
	
	DEFINE_VMETHOD_INTERPOSE(void, logic, ())
    {
        if (ui_advmode->menu == ui_advmode_menu::Inventory && ViewscreenAdvInventory::isActive)
		{
			//static int i = 0;
			//out->print("advinventory_hook::logic() %d\n", i++);
			ViewscreenAdvInventory::show();
			return;
		}

		INTERPOSE_NEXT(logic)();
	}
		
	DEFINE_VMETHOD_INTERPOSE(void, render, ())
    {
		if (ui_advmode->menu == ui_advmode_menu::Inventory && ViewscreenAdvInventory::isActive)
			return;

		INTERPOSE_NEXT(render)();

		//OutputString(0, 1, enum_item_key(ui_advmode->menu).append("  "));

		//std::stringstream ss;
		//ss << "0x" << std::hex << (int)Gui::getCurViewscreen() << " ";
		//OutputString(0, 1, ss.str());

		//std::stringstream ss;
		//ss << "0x" << std::hex << (int)getPlayerUnit() << " ";
		//OutputString(0, 1, ss.str());
		//ss.clear();
		//ss << "0x" << std::hex << (int)Gui::getSelectedUnit(*out, true) << " ";
		//OutputString(0, 2, ss.str());

		switch (ui_advmode->menu)
        {
			case ui_advmode_menu::Inventory:
			{
				int x = ViewscreenAdvInventory::getTabHotkeyStringX();
				OutputHotkeyString(x, 24, "Advanced Inventory", Screen::getKeyDisplay(A_INV_ADVINVENTORY), COLOR_LIGHTRED); // gps->dimy - 2
				OutputString(79, 23, "|");
				break;
			}
			default:
				return;
        }
    }

	/*void send_key(const df::interface_key &key)
    {
        set< df::interface_key > input;
        input.insert(key);
		Gui::getCurViewscreen(true)->feed(&input);
    }*/
};

IMPLEMENT_VMETHOD_INTERPOSE(advinventory_hook, feed);
IMPLEMENT_VMETHOD_INTERPOSE(advinventory_hook, logic);
IMPLEMENT_VMETHOD_INTERPOSE(advinventory_hook, render);


command_result advinventory (color_ostream &out, vector <string> & parameters)
{
    if (!parameters.empty())
        return CR_WRONG_USAGE;
    CoreSuspender suspend;
    out.print("Hello! I do nothing, remember?\n");
    return CR_OK;
}


DFhackCExport command_result plugin_enable ( color_ostream &out, bool enable)
{
    if (!gps) // || !gview
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(advinventory_hook, feed  ).apply(enable) ||
			!INTERPOSE_HOOK(advinventory_hook, logic ).apply(enable) ||
			!INTERPOSE_HOOK(advinventory_hook, render).apply(enable))
            return CR_FAILURE;

        is_enabled = enable;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
	::out = &out;
	commands.push_back(PluginCommand(
        "advinventory",
        "Replaces adventure mode inventory screens",
        advinventory,
        true, //allow non-interactive use
        "I am a long help string :)"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
	INTERPOSE_HOOK(advinventory_hook, feed  ).remove();
	INTERPOSE_HOOK(advinventory_hook, logic ).remove();
	INTERPOSE_HOOK(advinventory_hook, render).remove();
    return CR_OK;
}

// Called to notify the plugin about important state changes.
// Invoked with DF suspended, and always before the matching plugin_onupdate.
// More event codes may be added in the future.
DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    switch (event) {
		case SC_WORLD_LOADED:
			// initialize from the world just loaded
			break;
		case SC_WORLD_UNLOADED:
			// cleanup
			break;
		default:
			break;
    }
    return CR_OK;
}

// Whatever you put here will be done in each game step. Don't abuse it.
// It's optional, so you can just comment it out like this if you don't need it.
DFhackCExport command_result plugin_onupdate ( color_ostream &out )
{
    auto screen = Core::getTopViewscreen();

    if (strict_virtual_cast<df::viewscreen_dungeonmodest>(screen))
    {
        using namespace ui_advmode_menu;

        switch (ui_advmode->menu)
        {
			case Travel:
				break;
			default:
				break;
        }
    }
    return CR_OK;
}
