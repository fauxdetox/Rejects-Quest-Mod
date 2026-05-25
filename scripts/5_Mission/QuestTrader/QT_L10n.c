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

        string direct = ResolveDirectText(text);
        if (direct != "")
            return direct;

        string result = "";
        TStringArray lines = new TStringArray();
        text.Split("\n", lines);
        for (int li = 0; li < lines.Count(); li++)
        {
            if (li > 0) result = result + "\n";
            result = result + ResolveTextLine(lines[li]);
        }
        return result;
    }

    private static string ResolveTextLine(string line)
    {
        TStringArray parts = new TStringArray();
        line.Split(" ", parts);
        string result = "";
        for (int i = 0; i < parts.Count(); i++)
        {
            string part = parts[i];
            string resolved = ResolveSentenceToken(part);
            if (i > 0) result = result + " ";
            result = result + resolved;
        }
        return result;
    }

    private static string ResolveDirectText(string text)
    {
        if (text.IndexOf(" ") >= 0 || text.IndexOf("\n") >= 0 || text.IndexOf("\t") >= 0)
            return "";

        if (IsTranslationKey(text))
            return ResolveToken(text);

        return "";
    }

    private static string ResolveToken(string token)
    {
        string suffix = "";
        string key = token;
        while (key.Length() > 0)
        {
            string last = key.Substring(key.Length() - 1, 1);
            if (!IsTrailingPunctuation(last))
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

    private static string ResolveSentenceToken(string token)
    {
        string suffix = "";
        string key = token;
        while (key.Length() > 0)
        {
            string last = key.Substring(key.Length() - 1, 1);
            if (!IsTrailingPunctuation(last))
                break;
            suffix = last + suffix;
            key = key.Substring(0, key.Length() - 1);
        }

        if (IsTranslationKey(key))
            return ResolveToken(token);

        if (!LooksLikeClassName(key))
            return token;

        string resolved = ResolveClassToken(key);
        if (resolved == key)
            return token;

        return resolved + suffix;
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
        if (displayName == "$STR_DN_UNKNOWN" || displayName == "STR_DN_UNKNOWN")
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

    private static bool IsTranslationKey(string value)
    {
        if (value.IndexOf("$QuestTrader_") == 0) return true;
        if (value.IndexOf("#QuestTrader_") == 0) return true;
        if (value.IndexOf("$STR_") == 0) return true;
        if (value.IndexOf("#STR_") == 0) return true;
        return false;
    }

    private static bool IsTrailingPunctuation(string value)
    {
        if (value == ".") return true;
        if (value == ",") return true;
        if (value == ":") return true;
        if (value == ";") return true;
        if (value == "!") return true;
        if (value == "?") return true;
        if (value == ")") return true;
        if (value == "]") return true;
        if (value == "\"") return true;
        if (value == "'") return true;
        return false;
    }

    private static bool LooksLikeClassName(string value)
    {
        if (value == "") return false;

        bool hasUnderscore = value.IndexOf("_") >= 0;
        bool hasDigit = false;
        bool hasLower = false;
        bool hasUpper = false;
        string digits = "0123456789";
        string lower = "abcdefghijklmnopqrstuvwxyz";
        string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        for (int i = 0; i < value.Length(); i++)
        {
            string ch = value.Substring(i, 1);
            if (digits.IndexOf(ch) >= 0) hasDigit = true;
            if (lower.IndexOf(ch) >= 0) hasLower = true;
            if (upper.IndexOf(ch) >= 0) hasUpper = true;
        }

        string first = value.Substring(0, 1);
        bool startsUpper = (upper.IndexOf(first) >= 0);

        if (hasUnderscore || hasDigit) return true;
        if (hasUpper && hasLower && startsUpper) return true;
        return false;
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
        if (id == "MENU_OPEN_JOURNAL") return "Open quest journal";
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
        if (id == "REPEATABLE_AFTER") return "Repeatable after";
        if (id == "OBJECTIVE_COLLECT") return "Collect";
        if (id == "OBJECTIVE_DELIVER") return "Deliver";
        if (id == "OBJECTIVE_KILL") return "Kill";
        if (id == "OBJECTIVE_INTERACT") return "Interact with";
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
        if (id == "TOAST_COMPLETE_PREREQ") return "Complete prerequisite first:";
        if (id == "TOAST_NO_ACTIVE_CANCEL") return "No active quest to cancel.";
        if (id == "TOAST_QUEST_CANCELLED") return "Quest cancelled.";
        if (id == "TOAST_NOT_ACCEPTED") return "You have not accepted this quest.";
        if (id == "TOAST_INTERACTION_REQUIRED") return "You still need to interact with the quest object.";
        if (id == "TOAST_WRONG_INTERACTION_OBJECT") return "That is not the quest object.";
        if (id == "TOAST_ADMIN_WIPED_PLAYER") return "Your QuestTrader data has been wiped by an admin.";
        if (id == "TOAST_ADMIN_COMPLETED_FOR_YOU") return "A quest has been completed for you by an admin.";
        if (id == "TOAST_INCOMPLETE") return "Incomplete";
        if (id == "CHAT_PREFIX") return "[Quest]";
        if (id == "CHAT_TRADER") return "[Trader]";
        if (id == "CHAT_QUEST_ACCEPTED") return "Quest accepted";
        if (id == "CHAT_DELIVER_TO") return "Deliver to";
        if (id == "CHAT_TURN_IN_TO") return "Turn in to";
        if (id == "CHAT_NO_ACTIVE") return "No active quests. Find a Quest Trader!";
        if (id == "CHAT_COMPLETE_BEFORE_ACCEPT") return "Complete this quest before accepting";
        if (id == "CHAT_PROGRESS") return "Quest progress";
        if (id == "CHAT_KILLS") return "Kills";
        if (id == "CHAT_QUEST_COMPLETE") return "Quest complete";
        if (id == "CHAT_RETURN_REWARD") return "Return to the Quest Trader to claim your reward!";
        if (id == "CHAT_REWARDS") return "Rewards";
        if (id == "HUD_TITLE") return "[ QUESTS ]";
        if (id == "HUD_TOGGLE") return "Toggle";
        if (id == "HUD_TOGGLE_ACTION") return "Toggle";
        if (id == "HUD_DONE") return "[DONE!]";
        if (id == "HINT_INTERACT") return "See quests";
        if (id == "HINT_INTERACT_ACTION") return "See quests";
        if (id == "HINT_INTERACT_OBJECT") return "Interact";
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
        if (id == "JOURNAL_HINT") return "Scroll to read";
        if (id == "JOURNAL_HINT_SCROLL") return "Scroll to read - press";
        if (id == "JOURNAL_HINT_CLOSE") return "to close";
        if (id == "JOURNAL_LOADING") return "Loading journal entries...";
        if (id == "JOURNAL_EMPTY") return "No entries yet.";
        if (id == "JOURNAL_EMPTY_HINT") return "Complete quests to fill your journal.";
        if (id == "ADMIN_TITLE") return "QuestTrader Admin";
        if (id == "ADMIN_PLAYERS") return "PLAYERS";
        if (id == "ADMIN_LOGS") return "LOGS";
        if (id == "ADMIN_QUESTS") return "QUESTS";
        if (id == "ADMIN_QUEST_ID") return "Quest ID";
        if (id == "ADMIN_QUEST_ID_OPTIONAL") return "Quest ID or Title (blank allowed)";
        if (id == "ADMIN_QUEST_TITLE") return "Title";
        if (id == "ADMIN_QUEST_TRADER") return "Trader";
        if (id == "ADMIN_QUEST_TYPE") return "Type";
        if (id == "ADMIN_QUEST_DELIVERY_TRADER") return "Delivery";
        if (id == "ADMIN_QUEST_TASK_CLASS") return "Objective";
        if (id == "ADMIN_QUEST_AMOUNT") return "Qty";
        if (id == "ADMIN_QUEST_REWARD") return "Reward";
        if (id == "ADMIN_QUEST_REWARD_AMOUNT") return "Qty";
        if (id == "ADMIN_QUEST_REPEATABLE") return "Repeat";
        if (id == "ADMIN_QUEST_COOLDOWN") return "Cooldown";
        if (id == "ADMIN_QUEST_DELIVERY_ITEM") return "Delivery item";
        if (id == "ADMIN_QUEST_PREREQS") return "Prereqs";
        if (id == "ADMIN_QUEST_ACCEPT_MESSAGE") return "Accept msg";
        if (id == "ADMIN_QUEST_REWARD_MESSAGE") return "Reward msg";
        if (id == "ADMIN_QUEST_SPAWN_ITEM") return "Spawn item";
        if (id == "ADMIN_QUEST_SPAWN_POS") return "Spawn pos";
        if (id == "ADMIN_QUEST_NEW") return "NEW";
        if (id == "ADMIN_QUEST_SAVE") return "SAVE";
        if (id == "ADMIN_QUEST_REFRESH") return "REFRESH QUESTS";
        if (id == "ADMIN_QUESTS_LOADED") return "Quests loaded:";
        if (id == "ADMIN_QUEST_REQUIRED") return "Quest ID, title and trader are required.";
        if (id == "ADMIN_QUEST_SAVE_REQUESTED") return "Quest save requested...";
        if (id == "ADMIN_QUEST_HINT") return "Type: 0 Collect, 1 Kill, 2 Deliver. Objectives: class|amount|description|entity; Rewards: class|amount; Prereqs: quest_id;quest_id.";
        if (id == "ADMIN_RELOAD") return "RELOAD";
        if (id == "ADMIN_RESPAWN_NPCS") return "RESPAWN NPCS";
        if (id == "ADMIN_REFRESH_LOG") return "REFRESH LOG";
        if (id == "ADMIN_COMPLETE_QUEST") return "COMPLETE QUEST";
        if (id == "ADMIN_RESET_QUEST") return "RESET QUEST";
        if (id == "ADMIN_WIPE_PLAYER") return "WIPE PLAYER";
        if (id == "ADMIN_COPY_STEAMID") return "COPY";
        if (id == "ADMIN_SELECT_PLAYER") return "Select a player first.";
        if (id == "ADMIN_STEAMID_EMPTY") return "Selected player has no SteamID64.";
        if (id == "ADMIN_STEAMID_COPIED") return "SteamID64 copied";
        if (id == "ADMIN_ACCESS_DENIED") return "QuestTrader admin access denied.";
        if (id == "ADMIN_ALL_RESET") return "All player quest states reset.";
        if (id == "ADMIN_QUEST_RESET") return "Quest reset.";
        if (id == "ADMIN_QUEST_COMPLETED_PLAYER") return "Quest completed for player.";
        if (id == "ADMIN_INVALID_QUEST") return "Could not complete quest. Check quest ID/title and player state.";
        if (id == "ADMIN_PLAYER_WIPED") return "Player quests wiped.";
        if (id == "ADMIN_CONFIG_RELOADED") return "QuestTrader config reloaded.";
        if (id == "ADMIN_CONFIG_RELOAD_FAILED") return "QuestTrader config reload failed. Check logs.";
        if (id == "ADMIN_NPC_RESPAWN_STARTED") return "NPC respawn started.";
        if (id == "ADMIN_LOADED") return "Admin panel loaded. Requesting data...";
        if (id == "ADMIN_PLAYERS_LOAD_FAILED") return "Failed to load players.";
        if (id == "ADMIN_NO_ONLINE_PLAYERS") return "No players online.";
        if (id == "ADMIN_NO_LOGS") return "No logs found.";
        if (id == "ADMIN_HISTORY_TITLE") return "History";
        if (id == "ADMIN_HISTORY_LOADING") return "Loading history...";
        if (id == "ADMIN_HISTORY_EMPTY") return "No history found.";
        if (id == "ADMIN_HISTORY_ON_DEMAND") return "History: loaded on demand in pages.";
        if (id == "ADMIN_WAIT_PREVIOUS_ACTION") return "Wait for the previous action to finish...";
        if (id == "ADMIN_WAIT_REPEAT") return "Wait before repeating this action.";
        if (id == "ADMIN_LOADED_PREFIX") return "Loaded";
        if (id == "ADMIN_ONLINE_PLAYERS") return "online player(s)";
        if (id == "ADMIN_LOG_REFRESHED") return "Log refreshed";
        if (id == "ADMIN_LINES") return "lines";
        if (id == "ADMIN_REFRESHING_LOG") return "Refreshing log...";
        if (id == "ADMIN_RELOAD_REQUESTED") return "Config reload requested...";
        if (id == "ADMIN_RESPAWN_REQUESTED") return "NPC respawn requested...";
        if (id == "ADMIN_ENTER_QUEST_ID") return "Enter a Quest ID in the input field first.";
        if (id == "ADMIN_COMPLETE_DONE") return "Completed quest for";
        if (id == "ADMIN_RESET_DONE") return "Reset quest done for";
        if (id == "ADMIN_WIPED_DONE") return "Wiped quests for";
        if (id == "ADMIN_NAME") return "Name";
        if (id == "ADMIN_UID") return "UID";
        if (id == "ADMIN_STEAMID") return "SteamID";
        if (id == "ADMIN_ACTIVE") return "Active";
        if (id == "ADMIN_COMPLETED") return "Completed";
        if (id == "ADMIN_CONNECTED") return "Connected";
        if (id == "ADMIN_YES") return "yes";
        if (id == "ADMIN_POSITION") return "Position";
        if (id == "ADMIN_DB_FILE") return "DB file";
        if (id == "ADMIN_HISTORY_FILE") return "History file";
        if (id == "ADMIN_SUMMARY") return "Summary";
        if (id == "ADMIN_DB_SAVED") return "Quests saved in DB";
        if (id == "ADMIN_HISTORY_COMPLETED") return "Quests completed in history";
        if (id == "ADMIN_SAVED_STATES") return "Saved states";
        if (id == "ADMIN_NO_SAVED_QUESTS") return "No saved quests.";
        if (id == "ADMIN_MORE_SAVED_QUESTS") return "more saved quests";
        if (id == "ADMIN_RECENT_COMPLETED") return "Recent completed quests";
        if (id == "ADMIN_NO_HISTORY") return "No completion history registered.";
        if (id == "ADMIN_DETAILS_REDUCED") return "Details reduced to keep the admin panel light.";
        if (id == "ADMIN_UNKNOWN") return "Unknown";
        if (id == "ADMIN_NO_OBJECTIVES") return "no objectives";
        if (id == "ACTION_TALK") return "Talk";
        return id;
    }
}
