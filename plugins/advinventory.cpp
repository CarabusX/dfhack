#include <string>
#include <vector>
#include <set>

#include <VTableInterpose.h>

#include "Console.h"
#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"
#include <modules/Gui.h>
#include <modules/Screen.h>

#include "df/world.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/ui_advmode.h"
#include "df/interface_key.h"
#include "df/unit.h"
#include "df/unit_inventory_item.h"
#include "df/item.h"

using std::string;
using std::vector;
using std::set;

using namespace DFHack;
using namespace df::enums;

using df::global::gps;
using df::global::ui_advmode;


color_ostream* out;

void OutputString(int &x, int y, const std::string &text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK, bool rightAligned = false)
{
	int leftX = x;
	if (rightAligned) {
		leftX -= (text.length() - 1);
	} else {
		x += text.length();
	}
    Screen::paintString(Screen::Pen(' ', color, bg_color), leftX, y, text); //
}

inline void OutputString(const int &x, int y, const std::string &text, int8_t color = COLOR_WHITE, int8_t bg_color = COLOR_BLACK, bool rightAligned = false)
{
	int _x = x;
	OutputString(_x, y, text, color, bg_color, rightAligned);
}

void OutputHotkeyString(int &x, int y, const char *text, const std::string &hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE, bool addSemicolon = true)
{
    OutputString(x, y, hotkey, hotkey_color);
    string display( addSemicolon ? ": " : " " );
    display.append(text);
    OutputString(x, y, display, text_color);
}

void OutputHotkeyString(const int &x, int y, const char *text, const std::string &hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE, bool addSemicolon = true)
{
	int _x = x;
	OutputHotkeyString(_x, y, text, hotkey, hotkey_color, text_color, addSemicolon);
}


struct SortableItem {
	union {
		const df::unit_inventory_item* inv_item;
		df::item* item;
	};
	uint32_t sortingKey;
	int8_t color;

	vector< SortableItem* > containedItems;


	SortableItem (const df::unit_inventory_item* _inv_item)
		: inv_item(_inv_item)//, sortingKey( getSortingKey(_inv_item) )
	{
		sortingKey = 0;
		calculateKey_Mode_ItemType_BodyPart();
	}

	SortableItem (df::item* _item)
		: item(_item)
	{
		color = COLOR_LIGHTGREEN;
		sortingKey = 0;
		calculateKey_ItemType(item);
	}

	void calculateKey_Mode_ItemType_BodyPart() {
		uint32_t key;

		using df::unit_inventory_item;

		switch (inv_item->mode)
		{
			case unit_inventory_item::Weapon:
				color = COLOR_GREY;
				key = 0;
				break;
			case unit_inventory_item::Worn:
			case unit_inventory_item::Flask:
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

		sortingKey |= (key << (4 + 16));

		calculateKey_ItemType(inv_item->item);

		key = (uint32_t)( inv_item->body_part_id );
		sortingKey |= (key << (0));
	}

	void calculateKey_ItemType (df::item *item) {
		uint32_t key = getKey_ItemType(item);
		sortingKey |= (key << (16));
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
		//	default:
		//		return 9;
			case FLASK:
				return 10;
			case QUIVER:
				return 11;
			case BACKPACK:
				return 12;
			case BOX:
			case BIN:
				return 13;  //containers at the end
			default:
				return 9;
		}
	}
};


class ViewscreenAdvInventory : public dfhack_viewscreen
{
public:
	static bool isActive;


	static void show()
	{
		/*if (!instance)
			instance = new ViewscreenAdvInventory();
		else
			instance->parent = nullptr;
		Screen::show(instance);*/

		Screen::show( new ViewscreenAdvInventory() );
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
		else if (input->count(interface_key::CHANGETAB))
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
    }

    void render()
    {
        if (Screen::isDismissed(this))
            return;

        dfhack_viewscreen::render(); //calls check_resize();

        Screen::clear();
        //Screen::drawBorder("  Advanced Inventory  ");

		auto dim = Screen::getWindowSize();
		OutputString(0, 0, "Advanced Inventory");
		OutputString(dim.x - 2, 0, "DFHack", COLOR_LIGHTMAGENTA, COLOR_BLACK, true);

		OutputString(dim.x - 2, dim.y - 1, "Written by Carabus/Rafal99", COLOR_DARKGREY, COLOR_BLACK, true);

		int x = OutputHotkeysLine1(dim.y - 1);
		OutputHotkeyString(x, dim.y - 1, "Basic Inventory   ", Screen::getKeyDisplay(interface_key::CHANGETAB), COLOR_LIGHTRED); //gps->dimy - 2
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
		x += 2;
		return x;
	}
};

bool ViewscreenAdvInventory::isActive = false;
int ViewscreenAdvInventory::tabHotkeyStringX = -1;


DFHACK_PLUGIN("advinventory");

DFHACK_PLUGIN_IS_ENABLED(is_enabled);

struct advinventory_hook : df::viewscreen_dungeonmodest {
    typedef df::viewscreen_dungeonmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, feed, (set<df::interface_key> *input))
    {
        using namespace ui_advmode_menu;

        switch (ui_advmode->menu)
        {
			case Default:
				if (input->count(interface_key::CUSTOM_CTRL_I))
				{
					ViewscreenAdvInventory::isActive = true;
					input->clear();
					input->insert(interface_key::A_INV_LOOK);
				}
				break;
			case Inventory:
				if (input->count(interface_key::CHANGETAB))
				{
					ViewscreenAdvInventory::isActive = true;
					ViewscreenAdvInventory::show();
					return;
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

		//OutputString(0, 0, enum_item_key(ui_advmode->menu).append("  "));

 		using namespace ui_advmode_menu;
		
		switch (ui_advmode->menu)
        {
			case Inventory:
			{
				int x = ViewscreenAdvInventory::getTabHotkeyStringX();
				OutputHotkeyString(x, 24, "Advanced Inventory", Screen::getKeyDisplay(interface_key::CHANGETAB), COLOR_LIGHTRED); // gps->dimy - 2
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


command_result advinventory (color_ostream &out, std::vector <std::string> & parameters)
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

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
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