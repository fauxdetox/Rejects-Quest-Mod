// ============================================================
//  QuestTrader | QT_L10n.c
//  Client-side helpers for DayZ stringtable translations.
//  Quest/story text can stay in UTF-8 JSON; fixed UI text should
//  use QuestTrader_* keys from stringtable.csv.
// ============================================================

class QT_L10n
{
    static string Key(string id)
    {
        return "#QuestTrader_" + id;
    }

    static string T(string id)
    {
        string key = Key(id);
        string translated = Widget.TranslateString(key);
        if (translated == "" || translated == key || translated == "QuestTrader_" + id)
            return Fallback(id);
        return translated;
    }

    static string ResolveText(string text)
    {
        if (text == "") return "";

        TStringArray parts = new TStringArray();
        text.Split(" ", parts);
        string result = "";
        for (int i = 0; i < parts.Count(); i++)
        {
            string part = parts[i];
            string resolved = ResolveToken(part);
            if (i > 0) result = result + " ";
            result = result + resolved;
        }
        return result;
    }

    private static string ResolveToken(string token)
    {
        string suffix = "";
        string key = token;
        while (key.Length() > 0)
        {
            string last = key.Substring(key.Length() - 1, 1);
            if (last != "." && last != "," && last != ":" && last != ";" && last != ")" && last != "]")
                break;
            suffix = last + suffix;
            key = key.Substring(0, key.Length() - 1);
        }

        if (key.IndexOf("$QuestTrader_") == 0)
            key = "#" + key.Substring(1, key.Length() - 1);

        if (key.IndexOf("#QuestTrader_") == 0)
        {
            string translatedModKey = Widget.TranslateString(key);
            if (translatedModKey != "" && translatedModKey != key && translatedModKey != key.Substring(1, key.Length() - 1))
                return translatedModKey + suffix;
            string fallbackId = key.Substring(13, key.Length() - 13);
            return Fallback(fallbackId) + suffix;
        }

        if (key.IndexOf("$STR_") != 0 && key.IndexOf("#STR_") != 0)
            return ResolveClassToken(key) + suffix;

        string lookupKey = key;
        if (lookupKey.IndexOf("$STR_") == 0)
            lookupKey = "#" + lookupKey.Substring(1, lookupKey.Length() - 1);

        string translated = Widget.TranslateString(lookupKey);
        if (translated == "" || translated == lookupKey || translated == lookupKey.Substring(1, lookupKey.Length() - 1))
            return token;
        return translated + suffix;
    }

    private static string ResolveClassToken(string token)
    {
        if (token == "") return token;

        string displayName = "";
        GetGame().ConfigGetText("CfgVehicles " + token + " displayName", displayName);
        if (displayName == "")
            GetGame().ConfigGetText("CfgWeapons " + token + " displayName", displayName);
        if (displayName == "")
            GetGame().ConfigGetText("CfgMagazines " + token + " displayName", displayName);
        if (displayName == "")
            return token;

        if (displayName.IndexOf("$STR_") == 0)
        {
            string lookupKey = "#" + displayName.Substring(1, displayName.Length() - 1);
            string translated = Widget.TranslateString(lookupKey);
            if (translated != "" && translated != lookupKey && translated != lookupKey.Substring(1, lookupKey.Length() - 1))
                return translated;
        }

        return displayName;
    }

    static void ApplyText(Widget root, string widgetName, string locId)
    {
        if (!root) return;
        Widget widget = root.FindAnyWidget(widgetName);
        if (!widget) return;

        ButtonWidget button = ButtonWidget.Cast(widget);
        if (button)
        {
            button.SetText(Key(locId));
            return;
        }

        TextWidget text = TextWidget.Cast(widget);
        if (text) text.SetText(Key(locId));
    }

    static string Fallback(string id)
    {
        if (id == "MENU_TITLE") return "QUESTS";
        if (id == "MENU_AVAILABLE") return "AVAILABLE";
        if (id == "MENU_NOW") return "ACTIVE";
        if (id == "MENU_DONE") return "DONE";
        if (id == "MENU_DESCRIPTION") return "DESCRIPTION";
        if (id == "MENU_OBJECTIVES") return "OBJECTIVES";
        if (id == "MENU_REWARDS") return "REWARDS";
        if (id == "BUTTON_ACCEPT") return "ACCEPT";
        if (id == "BUTTON_COMPLETE") return "COMPLETE";
        if (id == "BUTTON_CANCEL") return "CANCEL";
        if (id == "STATE_AVAILABLE") return "Available";
        if (id == "STATE_ACTIVE") return "Active";
        if (id == "STATE_READY") return "Ready to turn in!";
        if (id == "STATE_COMPLETED") return "Completed";
        if (id == "STATE_COOLDOWN") return "On cooldown";
        if (id == "REQUIRES") return "Requires";
        if (id == "AVAILABLE_IN") return "Available in";
        if (id == "OBJECTIVE_COLLECT") return "Collect";
        if (id == "OBJECTIVE_DELIVER") return "Deliver";
        if (id == "OBJECTIVE_KILL") return "Kill";
        if (id == "OBJECTIVE_RECON") return "Recon";
        if (id == "OBJECTIVE_READY") return "Ready to deliver";
        if (id == "OBJECTIVES_ALL_READY") return "All collection items are in your inventory. Complete the quest.";
        if (id == "DELIVERY_ALL_READY") return "Delivery item is in your inventory. Deliver it to the target trader.";
        if (id == "QUEST_READY_TURN_IN") return "Quest ready to turn in";
        if (id == "TOAST_QUEST_ACCEPTED") return "Quest accepted:";
        if (id == "TOAST_STILL_NEED") return "Still need";
        if (id == "TOAST_DELIVER_THIS_TO") return "Deliver this to:";
        if (id == "TOAST_LOST_DELIVERY_ITEM") return "You lost the delivery item!";
        if (id == "TOAST_FINISH_CURRENT_TRADER") return "Finish your current quest from this trader first!";
        if (id == "TOAST_QUEST_NOT_AVAILABLE") return "Quest not available.";
        if (id == "HUD_TITLE") return "[ QUESTS ]";
        if (id == "HUD_TOGGLE") return "[O] Toggle";
        if (id == "HUD_DONE") return "[DONE!]";
        if (id == "HINT_INTERACT") return "[ Press F to see Quests ]";
        if (id == "RECON_SCANNING") return "Scanning";
        if (id == "RECON_REMAINING") return "s remaining";
        if (id == "RECON_START") return "Look through Binoculars to begin recon.";
        if (id == "RECON_PAUSED") return "Paused";
        if (id == "RECON_CONTINUE") return "Look through Binoculars to continue.";
        if (id == "LOG_TITLE") return "Quest Log";
        if (id == "LOG_HISTORY") return "HISTORY";
        if (id == "LOG_LEADERBOARD") return "LEADERBOARD";
        if (id == "LOG_EMPTY") return "No quests completed yet.";
        if (id == "LOG_NO_LEADERBOARD") return "No leaderboard data yet.";
        if (id == "LOG_TOP") return "-- Top Survivors --";
        if (id == "LOG_COMPLETED") return "Completed";
        if (id == "LOG_RUN") return "Run";
        if (id == "LOG_REWARDS_RECEIVED") return "Rewards received";
        if (id == "JOURNAL_TITLE") return "FIELD JOURNAL";
        if (id == "JOURNAL_HINT") return "Scroll to read - Press ' to close";
        if (id == "JOURNAL_LOADING") return "Loading journal entries...";
        if (id == "JOURNAL_EMPTY") return "No entries yet.";
        if (id == "JOURNAL_EMPTY_HINT") return "Complete quests to fill your journal.";
        if (id == "ADMIN_TITLE") return "QuestTrader Admin";
        if (id == "ADMIN_PLAYERS") return "PLAYERS";
        if (id == "ADMIN_LOGS") return "LOGS";
        if (id == "ADMIN_QUEST_ID") return "Quest ID or Title (blank = active)";
        if (id == "ADMIN_RELOAD") return "RELOAD";
        if (id == "ADMIN_RESPAWN_NPCS") return "RESPAWN NPCS";
        if (id == "ADMIN_REFRESH_LOG") return "REFRESH LOG";
        if (id == "ADMIN_RESET_QUEST") return "RESET QUEST";
        if (id == "ADMIN_COMPLETE_QUEST") return "COMPLETE QUEST";
        if (id == "ADMIN_COMPLETE_DONE") return "Completed quest for";
        if (id == "ADMIN_WIPE_PLAYER") return "WIPE PLAYER";
        if (id == "ADMIN_LOADED") return "Admin panel loaded. Requesting data...";
        if (id == "ADMIN_LOADED_PREFIX") return "Loaded";
        if (id == "ADMIN_ONLINE_PLAYERS") return "online player(s)";
        if (id == "ADMIN_LOG_REFRESHED") return "Log refreshed";
        if (id == "ADMIN_LINES") return "lines";
        if (id == "ADMIN_REFRESHING_LOG") return "Refreshing log...";
        if (id == "ADMIN_RELOAD_REQUESTED") return "Config reload requested...";
        if (id == "ADMIN_RESPAWN_REQUESTED") return "NPC respawn requested...";
        if (id == "ADMIN_ENTER_QUEST_ID") return "Enter a Quest ID in the input field first.";
        if (id == "ADMIN_RESET_DONE") return "Reset quest done for";
        if (id == "ADMIN_WIPED_DONE") return "Wiped quests for";
        if (id == "ADMIN_NAME") return "Name";
        if (id == "ADMIN_UID") return "UID";
        if (id == "ADMIN_ACTIVE") return "Active";
        if (id == "ADMIN_COMPLETED") return "Completed";
        return id;
    }
}
