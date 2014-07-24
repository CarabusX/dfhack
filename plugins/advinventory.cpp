#include <string>
#include <set>

#include <VTableInterpose.h>

#include "Console.h"
#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"
#include <modules/Screen.h>

#include "df/world.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/ui_advmode.h"
#include "df/interface_key.h"

using std::string;
using std::set;

using namespace DFHack;
using namespace df::enums;

using df::global::gps;
using df::global::ui_advmode;


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

void OutputHotkeyString(int &x, int y, const char *text, const std::string &hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE)
{
    OutputString(x, y, hotkey, hotkey_color);
    //string display(": ");
    //display.append(text);
	x++;
    OutputString(x, y, string(text), text_color);
}

void OutputHotkeyString(const int &x, int y, const char *text, const std::string &hotkey, int8_t hotkey_color = COLOR_LIGHTGREEN, int8_t text_color = COLOR_WHITE)
{
	int _x = x;
	OutputHotkeyString(_x, y, text, hotkey, hotkey_color, text_color);
}


DFHACK_PLUGIN("advinventory");

DFHACK_PLUGIN_IS_ENABLED(is_enabled);

class ViewscreenAdvInventory : public dfhack_viewscreen
{
public:
	ViewscreenAdvInventory()
	{
	}

    void feed(set<df::interface_key> *input)
    {
        if (input->count(interface_key::LEAVESCREEN))
        {
            input->clear();
            Screen::dismiss(this);
			ui_advmode->menu = ui_advmode_menu::Default;
            return;
        }
		else if (input->count(interface_key::CHANGETAB))
        {
			input->clear();
            Screen::dismiss(this);
			ui_advmode->menu = ui_advmode_menu::Inventory;
            //Screen::dismiss(Gui::getCurViewscreen(true));
            //Screen::show(new ViewscreenStocks());
			//Screen::show(df::viewscreen *screen, df::viewscreen *before)
			//Screen::show(new ViewscreenChooseMaterial(planner.getDefaultItemFilterForType(type)));
			//Gui::resetDwarfmodeView(true);
            //send_key(interface_key::D_VIEWUNIT);

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

		OutputString(0, 0, "Advanced Inventory");
		OutputString(gps->dimx - 1, 0, " DFHack ", COLOR_LIGHTMAGENTA, COLOR_BLACK, true);

		OutputString(gps->dimx - 1, gps->dimy - 1, "Written by Carabus/Rafal99", COLOR_DARKGREY, COLOR_BLACK, true);

		int x = OutputHotkeysLine1(gps->dimy - 1);
		OutputHotkeyString(x, gps->dimy - 1, "Basic Inventory", Screen::getKeyDisplay(interface_key::CHANGETAB), COLOR_LIGHTRED); //gps->dimy - 2
	}

	string getFocusString() { return "adv_inventory"; }
    //void onShow() {};

	static int getTabHotkeyStringX() {
		if (tabHotkeyStringX == -1) {
			tabHotkeyStringX = OutputHotkeysLine1(-1);
		}
		return tabHotkeyStringX;
	}

private:
	static int tabHotkeyStringX;

	static int OutputHotkeysLine1 (int y)
	{
		int x = 0;
		OutputHotkeyString(x, y, "to view other pages.", Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEUP) + Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEDOWN), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 2;
		OutputHotkeyString(x, y, "when done.", Screen::getKeyDisplay(interface_key::LEAVESCREEN), COLOR_LIGHTGREEN, COLOR_GREY);
		x += 2;
		return x;
	}
};

int ViewscreenAdvInventory::tabHotkeyStringX = -1;


struct viewscreen_advinventory : df::viewscreen_dungeonmodest {
    typedef df::viewscreen_dungeonmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, feed, (set<df::interface_key> *input))
    {
        using namespace ui_advmode_menu;

        switch (ui_advmode->menu)
        {
			case Default:
				if (input->count(interface_key::CUSTOM_CTRL_I))
				{
					Screen::show(new ViewscreenAdvInventory());
					return;
				}
				break;
			case Inventory:
				if (input->count(interface_key::CHANGETAB))
				{
					//Screen::dismiss(this);
					//Screen::dismiss(Gui::getCurViewscreen(true));
					Screen::show(new ViewscreenAdvInventory());
					return;
				}
				break;
			default:
				break;
        }

        INTERPOSE_NEXT(feed)(input);
    }
	
	DEFINE_VMETHOD_INTERPOSE(void, render, ())
    {
		INTERPOSE_NEXT(render)();

		//OutputString(0, 0, enum_item_key(ui_advmode->menu).append("  "));

		using namespace ui_advmode_menu;

        switch (ui_advmode->menu)
        {
			case Inventory:
			{
				//auto dim = Screen::getWindowSize();

				int x = ViewscreenAdvInventory::getTabHotkeyStringX();
				OutputHotkeyString(x, 24, "Advanced Inventory", Screen::getKeyDisplay(interface_key::CHANGETAB), COLOR_LIGHTRED); // gps->dimy - 2
				break;
			}
			default:
				return;
        }

    }
};

IMPLEMENT_VMETHOD_INTERPOSE(viewscreen_advinventory, feed);
IMPLEMENT_VMETHOD_INTERPOSE(viewscreen_advinventory, render);


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
        if (!INTERPOSE_HOOK(viewscreen_advinventory, feed  ).apply(enable) ||
			!INTERPOSE_HOOK(viewscreen_advinventory, render).apply(enable))
            return CR_FAILURE;

        is_enabled = enable;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
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
	INTERPOSE_HOOK(viewscreen_advinventory, feed  ).remove();
	INTERPOSE_HOOK(viewscreen_advinventory, render).remove();
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