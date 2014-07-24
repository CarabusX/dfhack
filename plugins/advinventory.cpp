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
#include <modules/Units.h>
#include <modules/Items.h>

#include "df/global_objects.h"
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


DFHACK_PLUGIN("advinventory");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

const string VERSION_STRING = "0.1.0";


color_ostream* out;

inline string intToString (int i)
{
	std::ostringstream ss;
	ss << i;
	return ss.str();
}

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

bool OutputString (int* x, int y, const string& text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK, bool rightAligned = false)
{
	int leftX;
	if (rightAligned) {
		*x -= text.length();
		leftX = *x + 1;
	} else {
		leftX = *x;
		*x += text.length();
	}
    return Screen::paintString( Screen::Pen(' ', color, bg_color), leftX, y, text );
}

inline bool OutputString (int x, int y, const string& text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK, bool rightAligned = false)
{
	return OutputString(&x, y, text, color, bg_color, rightAligned);
}

bool OutputLimitedString (int x, int y, int maxX, const string& text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK)
{
	int maxLength = maxX + 1 - x;
	if (maxLength < text.length()) {
		return OutputString(x, y, text.substr (0, maxLength), color, bg_color);
	} else {
		return OutputString(x, y, text, color, bg_color);
	}
}

void OutputHotkeyString (int* x, int y, const char* text, const string& hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE, bool addSemicolon = true)
{
    OutputString(x, y, hotkey, hotkey_color);
    string display( addSemicolon ? ": " : " " );
    display.append(text);
    OutputString(x, y, display, text_color);
}

void OutputHotkeyString (int x, int y, const char* text, const string& hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE, bool addSemicolon = true)
{
	OutputHotkeyString(&x, y, text, hotkey, hotkey_color, text_color, addSemicolon);
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


enum PossibleActions {
	POSSIBLE_DRINK,
	POSSIBLE_EAT,
	POSSIBLE_INTERACT,
	POSSIBLE_INTERACT_READ,
	POSSIBLE_WEARABLE,
	POSSIBLE_WORN,
	POSSIBLE_WEAR,
	NUM_POSSIBLE_ACTIONS,
	NUM_DISPLAYED_ACTION_FLAGS = 3
};

struct PossibleActionFlagInfo {
	const char tile;
	const int8_t color;
	const int8_t column;

	inline bool OutputFlagTile (int x, int y, int8_t bg_color = COLOR_BLACK) const
	{
		return OutputTile (x + column, y, tile, color, bg_color);
	}
};

PossibleActionFlagInfo possibleActionFlags [NUM_POSSIBLE_ACTIONS] = {
	{ 'D', COLOR_LIGHTBLUE, 0 },
	{ 'E', COLOR_BROWN    , 0 },
	{ 'I', COLOR_LIGHTRED , 1 },
	{ 'R', COLOR_WHITE    , 1 },
	{ 'W', COLOR_DARKGREY , 2 },
	{ 'W', COLOR_CYAN     , 2 },
	{ 'W', COLOR_LIGHTCYAN, 2 },
};


struct SortableItem;
typedef vector< SortableItem* const > SortableItemsVector;

struct Inventory;
struct SortableItem {
	static Inventory*& inventory;

	const df::unit_inventory_item *const inv_item;
	df::item *const item;
	string fullName;
	string bodyPartName;
	string weightString;

	string* treeString;
	string* treeStringSorted;

	int8_t color;
	int8_t indent;
	int totalChildren;
	//bool collapsed;
	bool possibleActions [NUM_POSSIBLE_ACTIONS];

	uint32_t sortingKey;
	SortableItemsVector* containedItems;
	SortableItemsVector* containedItemsSorted;

	//inline const SortableItemsVector* getContainedItems() const;
	inline void addToAllItems(); // not const because can't add const SortableItem* to vector
	void addToAllItemsSorted();


	SortableItem (const df::unit_inventory_item* _inv_item)	:
		inv_item(_inv_item), item(_inv_item->item),
		treeString(nullptr), treeStringSorted(nullptr),
		indent(0), totalChildren(0)/*, collapsed(false)*/
	{
		getItemBodyPartName(); // before adding to allItems so flasks won't iterate over themselves

		init();

		calculateKey_Mode_BodyPart();
	}

	SortableItem (df::item* _item, int8_t _indent) :
		inv_item(nullptr), item(_item),
		treeString(nullptr), treeStringSorted(nullptr),
		indent(_indent), totalChildren(0)/*, collapsed(false)*/
	{
		init();

		color = COLOR_LIGHTGREEN;
	}

	void init()
	{
		std::fill_n( possibleActions, (int)NUM_POSSIBLE_ACTIONS, false );

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

		getItemWeightString();
	}

	~SortableItem()
	{
		if (treeString) {
			delete treeString;
			delete treeStringSorted;
		}

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


	static void getItemTreeString (const SortableItemsVector& itemsVector, const string* prevString, bool sorted)
	{
		static const char* BRANCH_STRING = "\xC3""\x1A";

		const SortableItem* last = itemsVector.back();
		for (auto it = itemsVector.cbegin(); it != itemsVector.cend(); ++it)
		{
			SortableItem* sItem = *it;

			if (prevString) {
				string*& treeString_ = (sorted ? sItem->treeStringSorted : sItem->treeString);
				treeString_ = new string (*prevString);

				if (sItem == last)
				{
					(*treeString_) [ sItem->indent - 2 ] = '\xC0';
				}
			}

			if (sItem->containedItems) {
				const SortableItemsVector* containedItems_ = (sorted ? sItem->containedItemsSorted : sItem->containedItems);

				string nextString;
				if (prevString) {
					int8_t pos = sItem->indent - 2;
					nextString = (*prevString + BRANCH_STRING);
					nextString [pos]     = ( (sItem == last) ? ' ' : '\xB3' );
					nextString [pos + 1] = ' ';
				} else {
					nextString = BRANCH_STRING;
				}

				getItemTreeString(*containedItems_, &nextString, sorted);
			}
		}
	}


	static const string& getBodyPartName (int16_t body_part_id);
	static const SortableItem* getFlaskAttachedToItem (int16_t body_part_id);

	void getItemBodyPartName()
	{
		if (!inv_item)
			return;

		int16_t body_part_id = inv_item->body_part_id;

		bodyPartName = getBodyPartName (body_part_id);

		using df::unit_inventory_item;

		if (inv_item->mode == unit_inventory_item::Flask)
		{
			const SortableItem* attachedToItem = getFlaskAttachedToItem (body_part_id);
			if (attachedToItem)
			{
				bodyPartName = attachedToItem->fullName;
			}
		}

		Capitalize (bodyPartName);

		switch (inv_item->mode)
		{
			case unit_inventory_item::Piercing:
				bodyPartName = "In " + bodyPartName;
				break;
			case unit_inventory_item::StuckIn:
				bodyPartName = "Stuck in " + bodyPartName;
				break;
		}
	}


	void addItemWeight() const;

	void getItemWeightString()
	{
		if (!item->flags.bits.weight_computed)
			return;

		int32_t weight = item->weight;

		if (weight > 0) {
			weightString = intToString(weight) + '\xE2';
		} else {
			weightString = "<1""\xE2";
		}

		if (inv_item)
		{
			addItemWeight();
		}
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
		using df::unit_inventory_item;

		uint32_t key;

		switch (inv_item->mode)
		{
			case unit_inventory_item::Weapon:
				color = COLOR_GREY;
				key = 0;
				break;
			case unit_inventory_item::Worn:
			case unit_inventory_item::WrappedAround:
			case unit_inventory_item::Piercing:
			case unit_inventory_item::Flask:
				//possibleActions [POSSIBLE_WEARABLE] = true;
				color = COLOR_LIGHTCYAN;
				key = 1;
				break;
			case unit_inventory_item::StuckIn:
				color = COLOR_LIGHTRED;
				key = 2;
				break;
			default:
				color = COLOR_WHITE;
				key = 3;
		}

		sortingKey |= (key << (4 + 16 + 2 + 8)); //30

		key = (uint32_t)( inv_item->body_part_id );
		sortingKey |= (key << (2 + 8)); //10
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
		if (key > 200U) {
			key = std::min( 200U + (key - 200U) / 5U, 255U ); // if permit > 200, we only increase key by 1 every 5th permit
		}
		sortingKey |= (key << (0));
	}
};


struct Inventory {
	const df::unit* playerUnit;
	const vector< df::body_part_raw* >* playerBodyParts;
	int armorWeightMult;

	SortableItemsVector items;  // doesn't include items in containers
	SortableItemsVector itemsSorted;
	SortableItemsVector allItems;  // includes items in containers
	SortableItemsVector allItemsSorted;

	//inline const SortableItemsVector& getItems() const;
	inline const SortableItemsVector& getAllItems() const;

	int32_t totalWeight; // TODO: use int64_t instead of two variables?
	int32_t totalWeightFraction;
	int32_t effWeight;
	int32_t effWeightFraction;
	int32_t allowedWeight;
	string totalWeightString;
	string effWeightString;
	string allowedWeightString;


	Inventory()
		: totalWeight(0), totalWeightFraction(0), effWeight(0), effWeightFraction(0)
	{
		playerUnit = getPlayerUnit();

		int armorSkill;
		if (playerUnit) {
			playerBodyParts = &playerUnit->body.body_plan->body_parts;

			armorSkill = Units::getEffectiveSkill( (df::unit*)playerUnit, job_skill::ARMOR );

			int32_t bodySize = playerUnit->body.size_info.size_cur;
			int strengthValue = Units::getPhysicalAttrValue( (df::unit*)playerUnit, physical_attribute_type::STRENGTH );
			allowedWeight = std::max( 1, ( bodySize / 10 + strengthValue * 3 ) / 100 );
		} else {
			playerBodyParts = nullptr;
			armorSkill = 0;
			allowedWeight = 1;
		}
		armorWeightMult = std::max( 0, 15 - armorSkill );
	}

	void loadItems()
	{
		if (!playerUnit)
			return;

		auto &unitInventory = playerUnit->inventory;

		for (auto it = unitInventory.cbegin(); it != unitInventory.cend(); ++it)
		{
			const df::unit_inventory_item* inv_item = *it;

			SortableItem* sItem = new SortableItem(inv_item);
			items.push_back( sItem );
		}

		itemsSorted = items; //copy
		std::sort (itemsSorted.begin(), itemsSorted.end(), SortableItem::sortingFunction);

		for (auto it = itemsSorted.cbegin(); it != itemsSorted.cend(); ++it)
		{
			SortableItem* sItem = *it;
			sItem->addToAllItemsSorted();
		}

		SortableItem::getItemTreeString(items      , nullptr, false);
		SortableItem::getItemTreeString(itemsSorted, nullptr, true);

		  totalWeightString = weightToString(   totalWeight, totalWeightFraction );
		    effWeightString = weightToString(     effWeight,   effWeightFraction );
		allowedWeightString = weightToString( allowedWeight,                   0 );
	}

	~Inventory()
	{
		for (auto it = items.cbegin(); it != items.cend(); ++it)
		{
			delete *it;
		}
	}


	static string weightToString (int32_t weight, int32_t weightFraction)
	{
		if (weight == 0) {
			if (weightFraction == 0) {
				return "0""\xE2";
			} else if (weightFraction < 1000000) {
				return "<1""\xE2";
			}
		}

		weight += (weightFraction / 1000000);
		return ( intToString(weight) + '\xE2' );
	}
};


inline void SortableItem::addToAllItems() {
	inventory->allItems.push_back(this);
}

void SortableItem::addToAllItemsSorted() {
	SortableItemsVector& allItemsSorted = inventory->allItemsSorted;

	allItemsSorted.push_back(this);

	if (containedItemsSorted) {
		for (auto it = containedItemsSorted->cbegin(); it != containedItemsSorted->cend(); ++it)
		{
			SortableItem* sItem = *it;
			sItem->addToAllItemsSorted();
		}
	}
}

void SortableItem::addItemWeight() const {
	int32_t weight          = item->weight;
	int32_t weight_fraction = item->weight_fraction;

	inventory->totalWeight += weight;
	inventory->totalWeightFraction += weight_fraction;

	int weightMult = inventory->armorWeightMult;

	if ((inv_item->mode == df::unit_inventory_item::Worn ||
		 inv_item->mode == df::unit_inventory_item::WrappedAround) &&
		 item->isArmor() && weightMult < 15)
	{
		int64_t weight64 = (int64_t)weight * 1000000 + weight_fraction;
		weight64 = weight64 * weightMult / 15;

		weight          = weight64 / 1000000;
		weight_fraction = weight64 % 1000000;

		//weight          = weight          * weightMult / 15;
		//weight_fraction = weight_fraction * weightMult / 15;
	}

	inventory->effWeight += weight;
	inventory->effWeightFraction += weight_fraction;
}


const string& SortableItem::getBodyPartName (int16_t body_part_id)
{
	static const string EMPTY_STRING = "";

	if (!inventory->playerBodyParts)
		return EMPTY_STRING;

	df::body_part_raw* bodyPart = (*inventory->playerBodyParts) [ body_part_id ];
	return *(bodyPart->name_singular[0]);
}
	
const SortableItem* SortableItem::getFlaskAttachedToItem (int16_t body_part_id)
{
	const SortableItemsVector& allItems = inventory->allItems;

	for (auto it = allItems.cbegin(); it != allItems.cend(); ++it)
	{
		const SortableItem* sItem = *it;
		auto inv_item = sItem->inv_item;

		if (inv_item->mode == df::unit_inventory_item::Worn && inv_item->body_part_id == body_part_id)
		{
			return sItem;
		}
	}

	return nullptr;
}


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
	const bool implemented;
};

InventoryActionInfo inventoryActions [NUM_ACTIONS] = {
	{ interface_key::CUSTOM_SHIFT_D, "rop"    , true , true  },
	{ interface_key::CUSTOM_SHIFT_E, "at"     , false, false }, // "at or drink"
	{ interface_key::CUSTOM_SHIFT_G, "et"     , true , false },
	{ interface_key::CUSTOM_SHIFT_I, "nteract", false, false },
	{ interface_key::CUSTOM_SHIFT_P, "ut"     , true , false },
	{ interface_key::CUSTOM_SHIFT_R, "emove"  , true , false },
	{ interface_key::CUSTOM_SHIFT_T, "hrow"   , true , false },
	{ interface_key::CUSTOM_SHIFT_V, "iew"    , false, true  },
	{ interface_key::CUSTOM_SHIFT_W, "ear"    , false, false }
};

const df::interface_key A_INV_ADVINVENTORY = interface_key::CHANGETAB;


int32_t *const ui_item_view_page = (int32_t*)( (char*)df::global::ui_unit_view_mode + 8 );


class ViewscreenAdvInventoryData
{
	public:

};

class ViewscreenAdvInventory : public dfhack_viewscreen
{
public:
	static const int itemsListTopY     =  2;
	static const int itemsListBottomY_ = -8;
	static const int itemsListLeftX    =  0;
	static const int itemsListRightX_  = -1;

	static bool isActive; // is inventory screen in advanced mode
	static bool sortItems;
	static InventoryAction currentAction;
	static Inventory* inventory;
	static int firstIndex;


	static void initInventory()
	{
		inventory = new Inventory();
		inventory->loadItems();

		firstIndex = 0;
	}

	static void show()
	{
		/*if (!instance)
			instance = new ViewscreenAdvInventory();
		else
			instance->parent = nullptr;
		Screen::show(instance);*/

		if (!inventory)
		{
			initInventory();
		}

		Screen::show( new ViewscreenAdvInventory() );
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
				if (inventoryActions[i].implemented && input->count( inventoryActions[i].key ))
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
		x = 0;
		OutputString(&x, 0, "\x0F", COLOR_YELLOW);
		OutputString(&x, 0, "Advanced Inventory");
		OutputString(&x, 0, "\x0F", COLOR_YELLOW);

		OutputString(dim.x - 2, 0, "DFHack", COLOR_LIGHTMAGENTA, COLOR_BLACK, true);
		OutputString(dim.x - 2, dim.y - 2, "Version " + VERSION_STRING, COLOR_DARKGREY, COLOR_BLACK, true);
		OutputString(dim.x - 2, dim.y - 1, /*"Written "*/"by Carabus/Rafal99", COLOR_DARKGREY, COLOR_BLACK, true);

		x =  OutputBottomHotkeysLine(dim.y - 1);
		OutputHotkeyString(x, dim.y - 1, "Basic Inventory  ", Screen::getKeyDisplay(A_INV_ADVINVENTORY), COLOR_LIGHTRED); //gps->dimy - 2

		bool allowMultiSelect = inventoryActions[ currentAction ].allowMultiSelect;
		x = 0; y = dim.y - 2;
		OutputHotkeyString(&x, y, "Move cursor", "\x18""\x19", COLOR_LIGHTGREEN, COLOR_GREY);
		x += 3;
		OutputHotkeyString(&x, y, "Select item", Screen::getKeyDisplay(interface_key::SELECT), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 2;
		int selectRangeX = x;
		OutputHotkeyString(&x, y, "Select range", Screen::getKeyDisplay(interface_key::SELECT_ALL), ( allowMultiSelect ? COLOR_LIGHTGREEN : COLOR_GREEN ), ( allowMultiSelect ? COLOR_GREY : COLOR_DARKGREY ));
		x += 2;
		OutputHotkeyString(&x, y, "Deselect all  ", Screen::getKeyDisplay(interface_key::DESELECT_ALL), COLOR_LIGHTGREEN, COLOR_GREY);

		x = 0; y = dim.y - 3;
		/*OutputHotkeyString(&x, y, "Expand/Collapse", Screen::getKeyDisplay(interface_key::CUSTOM_SHIFT_C), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 3;
		OutputHotkeyString(&x, y, "Expand/Collapse All", Screen::getKeyDisplay(interface_key::CUSTOM_CTRL_C), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 3;*/
		x += 1;
		OutputHotkeyString(&x, y, sortItems ? "Items are sorted" : "Items are not sorted", Screen::getKeyDisplay(interface_key::CUSTOM_SHIFT_S), COLOR_LIGHTGREEN, ( sortItems ? COLOR_WHITE : COLOR_GREY ));
		//"Sort items" : "Do not sort items"
		x = selectRangeX + 10;
		OutputHotkeyString(&x, y, "Action on select", Screen::getKeyDisplay(interface_key::CUSTOM_SHIFT_A), ( allowMultiSelect ? COLOR_LIGHTGREEN : COLOR_GREEN ), ( allowMultiSelect ? COLOR_WHITE : COLOR_DARKGREY ));

		x = 0; y = dim.y - 4;
		OutputString(&x, y, "Actions:", COLOR_WHITE);
		x += 3;

		for (int i = 0; i < NUM_ACTIONS; i++) {
			InventoryActionInfo& action = inventoryActions[i];
			OutputString(&x, y, Screen::getKeyDisplay( action.key ), ( action.implemented ? COLOR_LIGHTGREEN : COLOR_GREY )); // COLOR_GREEN
			OutputString(&x, y, action.label, (i == currentAction) ? COLOR_LIGHTCYAN : ( action.implemented ? COLOR_GREY : COLOR_DARKGREY ));
			x += 3;
		}

		OutputTile(79, dim.y - 6, '\xB3', COLOR_DARKGREY);
	}

	void renderItemsList() {
		auto dim = Screen::getWindowSize();
		const int itemsListRightX  = ( (itemsListRightX_  < 0) ? dim.x : 0 ) + itemsListRightX_;
		const int itemsListBottomY = ( (itemsListBottomY_ < 0) ? dim.y : 0 ) + itemsListBottomY_;

		const int hotkeyX      = itemsListLeftX;
		const int selectedX    = hotkeyX   + 2;
		const int itemNameX    = selectedX + 2;
		int scrollBarX   = itemsListRightX;
		int actionFlagsX = scrollBarX   - 2 - NUM_DISPLAYED_ACTION_FLAGS;
		int weightX      = actionFlagsX - 2 -  5;
		int bodyPartX    = weightX      - 1 - 24; //25
		if (bodyPartX > 50)
		{
			bodyPartX    = 50;
			weightX      = bodyPartX    + 1 + 24;
			actionFlagsX = weightX      + 2 +  5;
			scrollBarX   = actionFlagsX + 2 + NUM_DISPLAYED_ACTION_FLAGS;
		}

		const SortableItemsVector& allItems = inventory->getAllItems();
		const int maxRows  = itemsListBottomY + 1 - itemsListTopY;
		const int itemRows = std::min( (int)allItems.size() - firstIndex, maxRows );
		int x, y;

		// hotkeys
		const int numItemHotkeys = std::min( itemRows, 26/* + 10*/ );
		static Screen::Pen hotkeyPen   (0     , COLOR_LIGHTGREEN, COLOR_BLACK);
		static Screen::Pen hotkeyBgPen ('\xFA', COLOR_LIGHTGREEN, COLOR_BLACK);
		for (int i = 0; i < numItemHotkeys; i++)
		{
			//char ch = char( (i < 26) ? ('a' + i) : ('0' + i - 26) );
			char ch = char('a' + i);
			Screen::paintTile( hotkeyPen.chtile(ch), hotkeyX, itemsListTopY + i );
		}
		if (numItemHotkeys < itemRows)
		{
			Screen::fillRect( hotkeyBgPen, hotkeyX, itemsListTopY + numItemHotkeys, hotkeyX, itemsListTopY + itemRows - 1 );
		}

		static const Screen::Pen flagsBgPen ('\x2D', COLOR_DARKGREY, COLOR_BLACK); // '\xFA' '\x2D'
		Screen::fillRect( flagsBgPen, actionFlagsX, itemsListTopY, actionFlagsX + NUM_DISPLAYED_ACTION_FLAGS - 1, itemsListTopY + itemRows - 1 );

		for (int i = 0; i < itemRows; i++)
		{
			const SortableItem* sItem = allItems[ firstIndex + i ];
			const int x = itemNameX + sItem->indent;
			const int y = itemsListTopY + i;

			int wx = weightX + 4;
			OutputString (&wx, y, sItem->weightString, ( sItem->inv_item ? COLOR_YELLOW : COLOR_BROWN ), COLOR_BLACK, true);
			OutputTile   (selectedX, y, '-', sItem->color);
			OutputLimitedString (x, y, bodyPartX - 2, sItem->fullName, sItem->color);
			OutputLimitedString (bodyPartX, y, std::min( wx - 1, weightX - 2 ), sItem->bodyPartName, sItem->color);

			// contained items tree
			if (sItem->treeString)
			{
				OutputString (itemNameX, y, ( sortItems ? *(sItem->treeStringSorted) : *(sItem->treeString) ), COLOR_GREEN);
			}

			/*if (sItem->totalChildren > 0) // default indentation using dots
			{
				static const Screen::Pen dotPen ('.', COLOR_LIGHTGREEN, COLOR_BLACK);
				int y2 = std::min( y + sItem->totalChildren, itemsListBottomY );
				Screen::fillRect(dotPen, x, y + 1, x, y2);
			}*/

			// possible action flags
			const bool* possibleActions = sItem->possibleActions;
			for (int i = 0; i < NUM_POSSIBLE_ACTIONS; i++) {
				if (possibleActions[i]) {
					possibleActionFlags[i].OutputFlagTile (actionFlagsX, y);
				}
			}
		}

		// column titles
		y = itemsListTopY - 1;
		OutputString (weightX - 1, y, "Weight", COLOR_WHITE, COLOR_BLACK); //weightX - 1
		//OutputString (weightX - 1, y, "Weight", COLOR_BLACK, COLOR_GREY);

		const int8_t actionFlagsBgColor = COLOR_BLACK; //COLOR_GREY
		possibleActionFlags[ POSSIBLE_EAT      ].OutputFlagTile (actionFlagsX, y, actionFlagsBgColor);
		possibleActionFlags[ POSSIBLE_INTERACT ].OutputFlagTile (actionFlagsX, y, actionFlagsBgColor);
		possibleActionFlags[ POSSIBLE_WEAR     ].OutputFlagTile (actionFlagsX, y, actionFlagsBgColor);

		// scrollbar
		static const Screen::Pen scrollbarPen ('\xBA', COLOR_DARKGREY, COLOR_BLACK); // '\xB3' '\xBA'
		Screen::fillRect(scrollbarPen, scrollBarX, itemsListTopY + 1, scrollBarX, itemsListBottomY - 1);
		
		int firstIndexRange = allItems.size() - maxRows;
		int8_t arrowColor = ( (firstIndexRange > 0) ? COLOR_WHITE : COLOR_DARKGREY );
		OutputTile (scrollBarX, itemsListTopY   , '\x1E', arrowColor); //   up arrow triangle
		OutputTile (scrollBarX, itemsListBottomY, '\x1F', arrowColor); // down arrow triangle

		// scrollbar button
		if (firstIndexRange > 0)
		{
			int buttonPos = 0;
			if (firstIndex > 0)
			{
				int scrollBarRange = maxRows - 3;
				buttonPos = 1 + std::min( firstIndex, firstIndexRange ) * (scrollBarRange - 1) / firstIndexRange;
			}

			OutputTile (scrollBarX, itemsListTopY + 1 + buttonPos, '\xF0', COLOR_YELLOW, COLOR_BROWN); // '\xF0' '\xFE'
		}

		x = 0; y = dim.y - 6;
		OutputString(&x, y, "Total items: ", COLOR_WHITE);
		OutputString(&x, y, intToString( allItems.size() ), COLOR_LIGHTGREEN);
		x += 4;
		OutputString(&x, y, "Weight:", COLOR_WHITE);
		x += 2;
		OutputString(&x, y, "Total: ", COLOR_GREY);
		OutputString(&x, y, inventory->totalWeightString, COLOR_YELLOW);
		x += 3;
		OutputString(&x, y, "Effective: ", COLOR_GREY);
		OutputString(&x, y, inventory->effWeightString, COLOR_YELLOW);
		x += 3;
		OutputString(&x, y, "Allowed: ", COLOR_GREY); //"Can carry: "
		OutputString(&x, y, inventory->allowedWeightString, COLOR_LIGHTGREEN);
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
			tabHotkeyStringX =  OutputBottomHotkeysLine(-1);
		}
		return tabHotkeyStringX;
	}

private:
	static int tabHotkeyStringX;

	static int OutputBottomHotkeysLine (int y)
	{
		int x = 0;
		OutputHotkeyString(&x, y, "to view other pages.",
			Screen::getKeyDisplay(interface_key::SECONDSCROLL_UP) +
			Screen::getKeyDisplay(interface_key::SECONDSCROLL_DOWN) +
			Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEUP) +
			Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEDOWN),
			COLOR_LIGHTGREEN, COLOR_GREY, false);
		x += 2;
		OutputHotkeyString(&x, y, "when done.",
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
Inventory*& SortableItem::inventory = ViewscreenAdvInventory::inventory;
int ViewscreenAdvInventory::firstIndex = 0;
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
		//ss << "0x" << std::hex << ui_advmode << "  ";
		//ss << "0x" << (ui_advmode + 1) << " ";
		//OutputString(0, 1, ss.str());
		//std::stringstream ss;
		//ss << "0x" << std::hex << Gui::getCurViewscreen(true) << " ";
		//OutputString(0, 1, ss.str());

		//std::stringstream ss;
		//ss << "0x" << std::hex << getPlayerUnit() << " ";
		//OutputString(0, 1, ss.str());
		//ss.clear();
		//ss << "0x" << std::hex << Gui::getSelectedUnit(*out, true) << " ";
		//OutputString(0, 2, ss.str());

		//OutputString(0, 1, intToString(*ui_item_view_page) + " ");

		switch (ui_advmode->menu)
        {
			case ui_advmode_menu::Inventory:
			{
				int x = ViewscreenAdvInventory::getTabHotkeyStringX();
				OutputHotkeyString(x, 24, "Advanced Inventory", Screen::getKeyDisplay(A_INV_ADVINVENTORY), COLOR_LIGHTRED); // gps->dimy - 2
				OutputTile(79, 23, '\xB3', COLOR_DARKGREY);
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
    out.print("AdvInventory: I am a GUI plugin, typing this command in DFHack widnow does nothing.\n");
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
