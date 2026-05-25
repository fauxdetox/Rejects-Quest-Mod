// ============================================================
//  QuestTrader | QuestConfigLoader.c  (v2.2)
//  Tries paths in order:
//    1. $profile:QuestTrader/QuestConfig.json  (user-edited copy)
//    2. QuestTrader/config/QuestConfig.json    (mod VFS path)
//  If neither exists, writes a default config to the profile.
//
//  Also loads QuestItemSettings.json for quantity-counted items
//  and QuestJournalStories.json for separated field-journal text.
// ============================================================

// ============================================================
//  Item settings — which classnames use GetQuantity() as count
//  and which reward items should be given as a single stack
// ============================================================
class QT_ItemSettings
{
    ref array<string> QuantityCountedItems;
    ref array<string> StackableRewardItems;
    ref array<string> DeerKillAliases;
    ref map<string, ref array<string>> KillTargetGroups;

    void QT_ItemSettings()
    {
        QuantityCountedItems = new array<string>();
        StackableRewardItems = new array<string>();
        DeerKillAliases      = new array<string>();
        KillTargetGroups     = new map<string, ref array<string>>();
    }
}

class QT_ItemSettingsLoader
{
    static const string PROFILE_PATH = "$profile:QuestTrader/QuestItemSettings.json";
    static const string MOD_PATH     = "QuestTrader/config/QuestItemSettings.json";

    static ref QT_ItemSettings LoadItemSettings()
    {
        string json = "";
        if (FileExist(PROFILE_PATH))
        {
            json = QT_JsonHelper.ReadFileToString(PROFILE_PATH);
            Print("[QuestTrader] Reading item settings from profile path.");
        }

        if (json == "" && FileExist(MOD_PATH))
        {
            json = QT_JsonHelper.ReadFileToString(MOD_PATH);
            Print("[QuestTrader] Reading item settings from mod path.");
            string dstDir = "$profile:QuestTrader/";
            if (!FileExist(dstDir)) MakeDirectory(dstDir);
            CopyFile(MOD_PATH, PROFILE_PATH);
        }

        if (json == "")
        {
            Print("[QuestTrader] No item settings found - writing defaults.");
            WriteDefaultItemSettings();
            json = QT_JsonHelper.ReadFileToString(PROFILE_PATH);
        }

        if (json == "") return BuildDefaultItemSettings();

        ref QT_ItemSettings settings;
        string err;
        JsonSerializer ser = new JsonSerializer();
        if (!ser.ReadFromString(settings, json, err))
        {
            Print("[QuestTrader] ERROR - item settings JSON parse failed: " + err);
            return BuildDefaultItemSettings();
        }

        if (!settings) return BuildDefaultItemSettings();
        if (!settings.QuantityCountedItems) settings.QuantityCountedItems = new array<string>();
        if (!settings.StackableRewardItems) settings.StackableRewardItems = new array<string>();
        if (!settings.DeerKillAliases) settings.DeerKillAliases = new array<string>();
        if (!settings.KillTargetGroups) settings.KillTargetGroups = new map<string, ref array<string>>();
        EnsureDefaultKillTargetGroups(settings);

        Print("[QuestTrader] Item settings loaded - " + settings.QuantityCountedItems.Count() + " quantity-counted, " + settings.StackableRewardItems.Count() + " stackable reward classnames.");
        return settings;
    }

    static void EnsureDefaultKillTargetGroups(QT_ItemSettings s)
    {
        if (!s || !s.KillTargetGroups) return;

        if (!s.KillTargetGroups.Contains("Deer"))
        {
            ref array<string> deerGroup = new array<string>();
            deerGroup.Insert("CervusElaphus");
            deerGroup.Insert("CapreolusCapreolus");
            s.KillTargetGroups.Insert("Deer", deerGroup);
        }

        if (!s.KillTargetGroups.Contains("Infected"))
        {
            ref array<string> infectedGroup = new array<string>();
            infectedGroup.Insert("ZombieBase");
            infectedGroup.Insert("Zmb");
            s.KillTargetGroups.Insert("Infected", infectedGroup);
        }

        if (!s.KillTargetGroups.Contains("Animals"))
        {
            ref array<string> animalGroup = new array<string>();
            animalGroup.Insert("AnimalBase");
            s.KillTargetGroups.Insert("Animals", animalGroup);
        }

        if (!s.KillTargetGroups.Contains("AI"))
        {
            ref array<string> aiGroup = new array<string>();
            aiGroup.Insert("ExpansionAI");
            aiGroup.Insert("eAIBase");
            aiGroup.Insert("SurvivorBase");
            s.KillTargetGroups.Insert("AI", aiGroup);
        }
    }

    static ref QT_ItemSettings BuildDefaultItemSettings()
    {
        ref QT_ItemSettings s = new QT_ItemSettings();
        // Quantity counted
        s.QuantityCountedItems.Insert("TetracyclineAntibiotics");
        s.QuantityCountedItems.Insert("PainkillerTablets");
        s.QuantityCountedItems.Insert("VitaminBottle");
        s.QuantityCountedItems.Insert("CharcoalTablets");
        s.QuantityCountedItems.Insert("Rag");
        s.QuantityCountedItems.Insert("WoodenPlank");
        s.QuantityCountedItems.Insert("PurificationTablets");
        // Stackable rewards
        s.StackableRewardItems.Insert("MoneyRuble1");
        s.StackableRewardItems.Insert("MoneyRuble5");
        s.StackableRewardItems.Insert("MoneyRuble10");
        s.StackableRewardItems.Insert("MoneyRuble25");
        s.StackableRewardItems.Insert("MoneyRuble50");
        s.StackableRewardItems.Insert("MoneyRuble100");
        s.StackableRewardItems.Insert("TetracyclineAntibiotics");
        s.StackableRewardItems.Insert("PainkillerTablets");
        s.StackableRewardItems.Insert("VitaminBottle");
        s.StackableRewardItems.Insert("CharcoalTablets");
        s.StackableRewardItems.Insert("Rag");
        s.StackableRewardItems.Insert("WoodenPlank");
        s.StackableRewardItems.Insert("PurificationTablets");
        // Kill aliases — deer quest counts any of these
        s.DeerKillAliases.Insert("CervusElaphus");
        s.DeerKillAliases.Insert("CapreolusCapreolus");
        EnsureDefaultKillTargetGroups(s);
        return s;
    }

    static void WriteDefaultItemSettings()
    {
        string dstDir = "$profile:QuestTrader/";
        if (!FileExist(dstDir)) MakeDirectory(dstDir);

        string defaultJson = "{\"QuantityCountedItems\":[\"TetracyclineAntibiotics\",\"PainkillerTablets\",\"VitaminBottle\",\"CharcoalTablets\",\"Rag\",\"WoodenPlank\",\"PurificationTablets\"],\"StackableRewardItems\":[\"MoneyRuble1\",\"MoneyRuble5\",\"MoneyRuble10\",\"MoneyRuble25\",\"MoneyRuble50\",\"MoneyRuble100\",\"TetracyclineAntibiotics\",\"PainkillerTablets\",\"VitaminBottle\",\"CharcoalTablets\",\"Rag\",\"WoodenPlank\",\"PurificationTablets\"],\"DeerKillAliases\":[\"CervusElaphus\",\"CapreolusCapreolus\"],\"KillTargetGroups\":{\"Deer\":[\"CervusElaphus\",\"CapreolusCapreolus\"],\"Infected\":[\"ZombieBase\",\"Zmb\"],\"Animals\":[\"AnimalBase\"],\"AI\":[\"ExpansionAI\",\"eAIBase\",\"SurvivorBase\"]}}";

        FileHandle fh = OpenFile(PROFILE_PATH, FileMode.WRITE);
        if (fh != 0)
        {
            FPrint(fh, defaultJson);
            CloseFile(fh);
            Print("[QuestTrader] Default item settings written to: " + PROFILE_PATH);
        }
        else
        {
            Print("[QuestTrader] ERROR - could not write default item settings.");
        }
    }
}

class QT_JournalStoriesConfig
{
    string _comment;
    ref array<string> order;
    ref map<string, string> stories;

    void QT_JournalStoriesConfig()
    {
        _comment = "QuestTrader Field Journal Stories. order = narrative sequence of quest IDs. stories = map of quest_id to story text.";
        order   = new array<string>();
        stories = new map<string, string>();
    }
}

class QT_JournalStoriesLoader
{
    static const string PROFILE_PATH = "$profile:QuestTrader/QuestJournalStories.json";
    static const string MOD_PATH     = "QuestTrader/config/QuestJournalStories.json";

    static ref QT_JournalStoriesConfig LoadStories()
    {
        string json = "";
        if (FileExist(PROFILE_PATH))
        {
            json = QT_JsonHelper.ReadFileToString(PROFILE_PATH);
            Print("[QuestTrader] Reading journal stories from profile path.");
        }

        if (json == "" && FileExist(MOD_PATH))
        {
            json = QT_JsonHelper.ReadFileToString(MOD_PATH);
            Print("[QuestTrader] Reading journal stories from mod path.");
            string dstDir = "$profile:QuestTrader/";
            if (!FileExist(dstDir)) MakeDirectory(dstDir);
            CopyFile(MOD_PATH, PROFILE_PATH);
        }

        if (json == "")
        {
            Print("[QuestTrader] No journal stories found - writing defaults.");
            WriteDefaultStories();
            json = QT_JsonHelper.ReadFileToString(PROFILE_PATH);
        }

        if (json == "") return new QT_JournalStoriesConfig();

        ref QT_JournalStoriesConfig cfg;
        string err;
        JsonSerializer ser = new JsonSerializer();
        if (!ser.ReadFromString(cfg, json, err))
        {
            Print("[QuestTrader] ERROR - journal stories JSON parse failed: " + err);
            cfg = new QT_JournalStoriesConfig();
            ParseStoriesMap(json, cfg.stories);
            ParseOrderArray(json, cfg.order);
            Print("[QuestTrader] Journal stories loaded through compatibility parser - " + cfg.stories.Count() + " stories, " + cfg.order.Count() + " ordered entries.");
            return cfg;
        }

        if (!cfg) cfg = new QT_JournalStoriesConfig();
        if (!cfg.stories) cfg.stories = new map<string, string>();
        if (!cfg.order)   cfg.order   = new array<string>();

        // JsonSerializer can't handle ref array<string>, parse order manually
        if (cfg.order.Count() == 0)
            ParseOrderArray(json, cfg.order);

        if (cfg.stories.Count() == 0)
            ParseStoriesMap(json, cfg.stories);

        Print("[QuestTrader] Journal stories loaded - " + cfg.stories.Count() + " stories, " + cfg.order.Count() + " ordered entries.");
        return cfg;
    }

    // Parse the "order" JSON array of strings manually
    private static void ParseOrderArray(string json, out array<string> result)
    {
        string QUOTE = "\"";
        int marker = json.IndexOf("order");
        if (marker < 0) return;
        string after = json.Substring(marker, json.Length() - marker);
        int arrOpen = after.IndexOf("[");
        if (arrOpen < 0) return;
        string arrContent = after.Substring(arrOpen + 1, after.Length() - arrOpen - 1);

        int pos = 0;
        int len = arrContent.Length();
        while (pos < len)
        {
            string ch = arrContent.Substring(pos, 1);
            if (ch == "]") break;
            if (ch != QUOTE) { pos++; continue; }
            pos++;
            int start = pos;
            while (pos < len && arrContent.Substring(pos, 1) != QUOTE) pos++;
            string entry = arrContent.Substring(start, pos - start);
            if (entry != "") result.Insert(entry);
            pos++;
        }
    }

    private static void ParseStoriesMap(string json, map<string, string> stories)
    {
        if (!stories) return;

        string QUOTE = "\"";
        int pos = 0;
        int len = json.Length();

        while (pos < len)
        {
            string marker = QUOTE + "quest_";
            string remaining = json.Substring(pos, len - pos);
            int rel = remaining.IndexOf(marker);
            if (rel < 0) break;

            int keyStart = pos + rel + 1;
            int keyEnd = keyStart;
            while (keyEnd < len && json.Substring(keyEnd, 1) != QUOTE) keyEnd++;
            if (keyEnd >= len) break;

            string questId = json.Substring(keyStart, keyEnd - keyStart);
            int afterKey = keyEnd + 1;
            while (afterKey < len)
            {
                string ws = json.Substring(afterKey, 1);
                if (ws != " " && ws != "\t" && ws != "\r" && ws != "\n") break;
                afterKey++;
            }

            if (afterKey >= len || json.Substring(afterKey, 1) != ":")
            {
                pos = keyEnd + 1;
                continue;
            }

            int valueStart = afterKey + 1;
            while (valueStart < len)
            {
                string valueWs = json.Substring(valueStart, 1);
                if (valueWs != " " && valueWs != "\t" && valueWs != "\r" && valueWs != "\n") break;
                valueStart++;
            }

            if (valueStart >= len || json.Substring(valueStart, 1) != QUOTE)
            {
                pos = keyEnd + 1;
                continue;
            }

            valueStart++;
            int valueEnd = valueStart;
            while (valueEnd < len)
            {
                string ch = json.Substring(valueEnd, 1);
                if (ch == QUOTE) break;
                valueEnd++;
            }

            string story = json.Substring(valueStart, valueEnd - valueStart);
            if (questId != "" && story != "")
                stories.Set(questId, story);

            pos = valueEnd + 1;
        }
    }

    static void WriteDefaultStories()
    {
        string dstDir = "$profile:QuestTrader/";
        if (!FileExist(dstDir)) MakeDirectory(dstDir);

        string defaultJson = "{\"_comment\":\"QuestTrader Field Journal Stories - maps quest_id to journal story text shown on completion. Edit freely - changes take effect on server restart or config reload.\",\"stories\":{\"quest_001\":\"The bandages reached Viktor before sundown. It was not enough to fix the world, but it was enough to keep one more person breathing through the night.\"}}";

        FileHandle fh = OpenFile(PROFILE_PATH, FileMode.WRITE);
        if (fh != 0)
        {
            FPrint(fh, defaultJson);
            CloseFile(fh);
            Print("[QuestTrader] Default journal stories written to: " + PROFILE_PATH);
        }
        else
        {
            Print("[QuestTrader] ERROR - could not write default journal stories.");
        }
    }
}

class QT_ConfigLoader
{
    static const string PROFILE_PATH = "$profile:QuestTrader/QuestConfig.json";
    static const string MOD_PATH     = "QuestTrader/config/QuestConfig.json";

    static ref QT_Config LoadConfig()
    {
        // Try profile path first (user may have edited it there)
        string json = "";
        if (FileExist(PROFILE_PATH))
        {
            json = QT_JsonHelper.ReadFileToString(PROFILE_PATH);
            Print("[QuestTrader] Reading config from profile path.");
        }

        // Fall back to mod VFS path
        if (json == "" && FileExist(MOD_PATH))
        {
            json = QT_JsonHelper.ReadFileToString(MOD_PATH);
            Print("[QuestTrader] Reading config from mod path.");
            // Copy to profile for user to edit
            string dstDir = "$profile:QuestTrader/";
            if (!FileExist(dstDir)) MakeDirectory(dstDir);
            CopyFile(MOD_PATH, PROFILE_PATH);
        }

        // If still nothing, write a minimal default directly
        if (json == "")
        {
            Print("[QuestTrader] No config found - writing default to profile.");
            WriteDefaultConfig();
            json = QT_JsonHelper.ReadFileToString(PROFILE_PATH);
        }

        if (json == "")
        {
            Print("[QuestTrader] ERROR - could not load any config.");
            return null;
        }

        ref QT_Config cfg;
        string err;
        JsonSerializer ser = new JsonSerializer();
        if (!ser.ReadFromString(cfg, json, err))
        {
            Print("[QuestTrader] ERROR - JSON parse failed: " + err);
            return null;
        }

        if (!cfg)
        {
            Print("[QuestTrader] ERROR - config null after parse.");
            return null;
        }

        if (!cfg.AdminSteamIds)
            cfg.AdminSteamIds = new array<string>();
        if (!cfg.TraderNPCPositions)
            cfg.TraderNPCPositions = new array<ref QT_TraderDef>();
        if (!cfg.Quests)
            cfg.Quests = new array<ref QT_QuestDef>();
        if (!cfg.Settings)
            cfg.Settings = new QT_Settings();
        if (json.IndexOf("\"ActivateSucessSound\"") < 0)
            cfg.Settings.ActivateSucessSound = true;
        if (json.IndexOf("\"EnableQuestChatMessages\"") < 0)
            cfg.Settings.EnableQuestChatMessages = true;
        if (json.IndexOf("\"EnableKillDebug\"") < 0)
            cfg.Settings.EnableKillDebug = cfg.Settings.debugLogging;
        if (json.IndexOf("\"EnableQuestTraderPerformanceLogs\"") < 0)
            cfg.Settings.EnableQuestTraderPerformanceLogs = false;
        if (json.IndexOf("\"QuestTraderEventDebounceSeconds\"") < 0 || cfg.Settings.QuestTraderEventDebounceSeconds <= 0)
            cfg.Settings.QuestTraderEventDebounceSeconds = 3.0;
        if (json.IndexOf("\"QuestTraderSaveDebounceSeconds\"") < 0 || cfg.Settings.QuestTraderSaveDebounceSeconds <= 0)
            cfg.Settings.QuestTraderSaveDebounceSeconds = 10.0;
        if (json.IndexOf("\"QuestTraderMaxRpcItemsPerPacket\"") < 0 || cfg.Settings.QuestTraderMaxRpcItemsPerPacket <= 0)
            cfg.Settings.QuestTraderMaxRpcItemsPerPacket = 50;
        if (json.IndexOf("\"QuestSaveDebounceSeconds\"") < 0 || cfg.Settings.QuestSaveDebounceSeconds <= 0)
            cfg.Settings.QuestSaveDebounceSeconds = 15.0;
        if (json.IndexOf("\"QuestSaveMaxPlayersPerTick\"") < 0 || cfg.Settings.QuestSaveMaxPlayersPerTick <= 0)
            cfg.Settings.QuestSaveMaxPlayersPerTick = 2;
        if (json.IndexOf("\"QuestFullBackupIntervalSeconds\"") < 0 || cfg.Settings.QuestFullBackupIntervalSeconds <= 0)
            cfg.Settings.QuestFullBackupIntervalSeconds = 900.0;

        foreach (QT_QuestDef def : cfg.Quests)
        {
            if (!def) continue;
            if (!def.objectives) def.objectives = new array<ref QT_Objective>();
            if (!def.Targets) def.Targets = new array<string>();
            if (!def.rewards) def.rewards = new array<ref QT_Reward>();
            if (!def.prerequisiteQuestIds) def.prerequisiteQuestIds = new array<string>();
            if (!def.spawnItems) def.spawnItems = new array<ref QT_SpawnItem>();

            if (def.type == QT_QuestType.KILL && def.objectives.Count() == 0 && def.Targets.Count() > 0)
            {
                ref QT_Objective shortcutObj = new QT_Objective();
                foreach (string shortcutTarget : def.Targets)
                    shortcutObj.Targets.Insert(shortcutTarget);
                shortcutObj.requiredAmount = def.RequiredAmount;
                if (shortcutObj.requiredAmount <= 0) shortcutObj.requiredAmount = 1;
                shortcutObj.description = "Kill " + shortcutObj.requiredAmount.ToString() + " target(s)";
                def.objectives.Insert(shortcutObj);
            }

            foreach (QT_Objective obj : def.objectives)
            {
                if (obj && !obj.Targets)
                    obj.Targets = new array<string>();
            }
        }

        Print("[QuestTrader] Config loaded - " + cfg.TraderNPCPositions.Count() + " traders, " + cfg.Quests.Count() + " quests.");
        return cfg;
    }

    static bool SaveConfig(QT_Config cfg)
    {
        if (!cfg) return false;

        string dstDir = "$profile:QuestTrader/";
        if (!FileExist(dstDir)) MakeDirectory(dstDir);

        bool ok = QT_JsonHelper.SaveToFile(PROFILE_PATH, cfg);
        if (ok)
            Print("[QuestTrader] Config saved to profile path.");
        return ok;
    }

    // --------------------------------------------------------
    //  Write a minimal default config to the profile directory
    //  so the server can start even without the mod files copied
    // --------------------------------------------------------
    static void WriteDefaultConfig()
    {
        string dstDir = "$profile:QuestTrader/";
        if (!FileExist(dstDir)) MakeDirectory(dstDir);

        // Minimal working config - 1 trader near Elektro, 1 quest
        string defaultJson = "{\"AdminSteamIds\":[],\"TraderNPCPositions\":[{\"id\":\"trader_001\",\"name\":\"Viktor\",\"position\":[3692.0,0.0,5988.0],\"orientation\":[0.0,180.0,0.0],\"model\":\"SurvivorM_Mirek\",\"greeting\":\"Hello survivor. I have work for you.\",\"farewell\":\"Good luck.\"}],\"Quests\":[{\"id\":\"quest_001\",\"traderId\":\"trader_001\",\"title\":\"Medical Supplies\",\"description\":\"Bring me bandages.\",\"journalStory\":\"The bandages reached Viktor before sundown. It was not enough to fix the world, but it was enough to keep one more person breathing through the night.\",\"type\":0,\"repeatable\":true,\"cooldownHours\":1,\"objectives\":[{\"itemClassName\":\"Bandage\",\"requiredAmount\":3,\"description\":\"Collect 3 Bandages\"}],\"rewards\":[{\"itemClassName\":\"AKM\",\"amount\":1}],\"rewardMessage\":\"Well done!\",\"acceptMessage\":\"Bring me 3 bandages.\",\"prerequisiteQuestIds\":[]}],\"Settings\":{\"interactionDistance\":3.5,\"showQuestMarkersOnMap\":true,\"notifyOnKillProgress\":true,\"EnableKillDebug\":false,\"debugLogging\":true,\"ActivateSucessSound\":true,\"EnableQuestChatMessages\":true,\"EnableQuestTraderPerformanceLogs\":false,\"QuestTraderEventDebounceSeconds\":3,\"QuestTraderSaveDebounceSeconds\":10,\"QuestTraderMaxRpcItemsPerPacket\":50,\"QuestSaveDebounceSeconds\":15,\"QuestSaveMaxPlayersPerTick\":2,\"QuestFullBackupIntervalSeconds\":900}}";

        FileHandle fh = OpenFile(PROFILE_PATH, FileMode.WRITE);
        if (fh != 0)
        {
            FPrint(fh, defaultJson);
            CloseFile(fh);
            Print("[QuestTrader] Default config written to: " + PROFILE_PATH);
        }
        else
        {
            Print("[QuestTrader] ERROR - could not write default config.");
        }
    }
}
