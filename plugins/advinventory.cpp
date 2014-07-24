#include "Console.h"
#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"
#include <modules/Screen.h>

#include <VTableInterpose.h>

#include "df/world.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/ui_advmode.h"

using namespace DFHack;
using namespace df::enums;

using df::global::gps;
using df::global::ui_advmode;
using df::enums::ui_advmode_menu;


/*void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}*/

void OutputString(int &x, int y, const std::string &text, int8_t color = COLOR_WHITE, const UIColor bg_color = COLOR_BLACK)
{
    Screen::paintString(Screen::Pen(' ', color, bg_color), x, y, text);
    x += text.length();
}

void OutputHotkeyString(int &x, int y, const char *text, const char *hotkey, int8_t text_color = COLOR_WHITE, int8_t hotkey_color = COLOR_LIGHTGREEN)
{
    OutputString(hotkey_color, x, y, hotkey);
    string display(": ");
    display.append(text);
    OutputString(text_color, x, y, display, newline, left_margin);
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
            return;
        }
		else if (input->count(interface_key::CHANGETAB))
        {
			input->clear();
            Screen::dismiss(this);
			ui_advmode->menu = ui_advmode_menu::Inventory;
			//Screen::show(df::viewscreen *screen, df::viewscreen *before)
            return;
        }
    }

    void render()
    {
        if (Screen::isDismissed(this))
            return;

        dfhack_viewscreen::render(); //calls check_resize();

        Screen::clear();
        Screen::drawBorder("  Advanced Inventory  ");

		OutputHotkeyString(40, gps->dimy - 2, "Basic Inventory", "Tab");
	}

    void onShow() {};
};


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

		Screen::Pen pen(' ', COLOR_WHITE, COLOR_BLACK);
		Screen::paintString(pen, 0, 0, enum_item_key(ui_advmode->menu));

		using namespace ui_advmode_menu;

        switch (ui_advmode->menu)
        {
			case Inventory:
				//auto dim = Screen::getWindowSize();
				OutputHotkeyString(40, gps->dimy - 2, "Advanced Inventory", "Tab");
				break;
			default:
				return;
        }

    }
};

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