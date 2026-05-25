// ============================================================
//  QuestTrader | QT_Input.c
//  Remappable DayZ input action helpers. Actions are declared
//  in QuestTrader/inputs.xml under the QuestTrader category.
// ============================================================

static const string QT_INPUT_OPEN_QUEST_MENU = "UAQuestTraderOpenQuestMenu";
static const string QT_INPUT_TOGGLE_HUD      = "UAQuestTraderToggleHUD";
static const string QT_INPUT_OPEN_ADMIN      = "UAQuestTraderOpenAdmin";
static const string QT_INPUT_OPEN_JOURNAL    = "UAQuestTraderOpenJournal";

class QT_Input
{
    static bool LocalPress(string inputName)
    {
        if (!GetUApi()) return false;

        UAInput input = GetUApi().GetInputByName(inputName);
        if (!input) return false;

        return input.LocalPress();
    }

    static string GetBoundKeyName(string inputName)
    {
        if (!GetUApi()) return inputName;

        string keyName = InputUtils.GetButtonNameFromInput(inputName, EUAINPUT_DEVICE_KEYBOARDMOUSE);
        if (keyName == "")
            keyName = inputName;
        return keyName;
    }

    static bool ShouldBlockQuestTraderShortcut()
    {
        UIManager ui = GetGame().GetUIManager();
        if (!ui) return false;

        // Check for chat menus by ID
        if (ui.FindMenu(MENU_CHAT_INPUT) || ui.FindMenu(MENU_CHAT))
            return true;

        // If game input has no focus, something has captured it (chat, another menu, etc.)
        Input inp = GetGame().GetInput();
        if (inp && !inp.HasGameFocus())
            return true;

        UIScriptedMenu currentMenu = ui.GetMenu();
        if (currentMenu)
        {
            string menuClass = currentMenu.ClassName();
            if (IsQuestTraderMenuClass(menuClass))
                return false;

            return true;
        }

        return false;
    }

    private static bool IsQuestTraderMenuClass(string menuClass)
    {
        if (menuClass == "QT_QuestMenu") return true;
        if (menuClass == "QT_QuestLog") return true;
        if (menuClass == "QT_Journal") return true;
        if (menuClass == "QT_AdminPanel") return true;
        return false;
    }
}
