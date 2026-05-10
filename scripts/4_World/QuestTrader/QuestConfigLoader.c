// ============================================================
//  QuestTrader | QuestConfigLoader.c  (v2.2)
//  Tries paths in order:
//    1. $profile:QuestTrader/QuestConfig.json  (user-edited copy)
//    2. QuestTrader/config/QuestConfig.json    (mod VFS path)
//  If neither exists, writes a default config to the profile.
//
//  Also loads QuestItemSettings.json for quantity-counted items.
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

    void QT_ItemSettings()
    {
        QuantityCountedItems = new array<string>();
        StackableRewardItems = new array<string>();
        DeerKillAliases      = new array<string>();
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

        Print("[QuestTrader] Item settings loaded - " + settings.QuantityCountedItems.Count() + " quantity-counted, " + settings.StackableRewardItems.Count() + " stackable reward classnames.");
        return settings;
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
        return s;
    }

    static void WriteDefaultItemSettings()
    {
        string dstDir = "$profile:QuestTrader/";
        if (!FileExist(dstDir)) MakeDirectory(dstDir);

        string defaultJson = "{\"QuantityCountedItems\":[\"TetracyclineAntibiotics\",\"PainkillerTablets\",\"VitaminBottle\",\"CharcoalTablets\",\"Rag\",\"WoodenPlank\",\"PurificationTablets\"],\"StackableRewardItems\":[\"MoneyRuble1\",\"MoneyRuble5\",\"MoneyRuble10\",\"MoneyRuble25\",\"MoneyRuble50\",\"MoneyRuble100\",\"TetracyclineAntibiotics\",\"PainkillerTablets\",\"VitaminBottle\",\"CharcoalTablets\",\"Rag\",\"WoodenPlank\",\"PurificationTablets\"]}";

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

        Print("[QuestTrader] Config loaded - " + cfg.TraderNPCPositions.Count() + " traders, " + cfg.Quests.Count() + " quests.");
        return cfg;
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
        string defaultJson = "{\"AdminSteamIds\":[],\"TraderNPCPositions\":[{\"id\":\"trader_001\",\"name\":\"Viktor\",\"position\":[3692.0,0.0,5988.0],\"orientation\":[0.0,180.0,0.0],\"model\":\"SurvivorM_Mirek\",\"greeting\":\"Hello survivor. I have work for you.\",\"farewell\":\"Good luck.\"}],\"Quests\":[{\"id\":\"quest_001\",\"traderId\":\"trader_001\",\"title\":\"Medical Supplies\",\"description\":\"Bring me bandages.\",\"type\":0,\"repeatable\":true,\"cooldownHours\":1,\"objectives\":[{\"itemClassName\":\"Bandage\",\"requiredAmount\":3,\"description\":\"Collect 3 Bandages\"}],\"rewards\":[{\"itemClassName\":\"AKM\",\"amount\":1}],\"rewardMessage\":\"Well done!\",\"acceptMessage\":\"Bring me 3 bandages.\",\"prerequisiteQuestIds\":[]}],\"Settings\":{\"interactionDistance\":3.5,\"showQuestMarkersOnMap\":true,\"notifyOnKillProgress\":true,\"debugLogging\":true}}";

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
