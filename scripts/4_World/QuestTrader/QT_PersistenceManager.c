// ============================================================
//  QuestTrader | QT_PersistenceManager.c  (v3.0)
//  Avoids "out" keyword for complex types (map) - returns
//  the loaded map directly instead of via out parameter.
// ============================================================

class QT_SavedObjective
{
    int currentAmount;
}

class QT_SavedQuestState
{
    string questId;
    int    state;
    ref array<ref QT_SavedObjective> objectives;
    int    completedTimestamp;

    void QT_SavedQuestState() { objectives = new array<ref QT_SavedObjective>(); }
}

class QT_SavedPlayerData
{
    string playerUID;
    ref array<ref QT_SavedQuestState> questStates;

    void QT_SavedPlayerData() { questStates = new array<ref QT_SavedQuestState>(); }
}

class QT_PersistenceManager
{
    private static const string SAVE_DIR = "$profile:QuestTrader/players/";
    private static ref map<string, string> s_lastSavedSignatures;

    static bool SavePlayer(string uid, map<string, ref QT_PlayerQuestState> questMap)
    {
        if (uid == "" || !questMap) return false;
        EnsureSaveState();

        string signature = BuildQuestMapSignature(questMap);
        if (s_lastSavedSignatures.Contains(uid) && s_lastSavedSignatures.Get(uid) == signature)
        {
            QT_Perf.Log("save skipped uid=" + uid + " reason=unchanged");
            return true;
        }

        if (!WritePlayer(uid, questMap))
            return false;

        s_lastSavedSignatures.Set(uid, signature);
        QT_Perf.Log("Player saved successfully uid=" + uid + " quests=" + questMap.Count().ToString());
        return true;
    }

    static bool ForceSavePlayer(string uid, map<string, ref QT_PlayerQuestState> questMap)
    {
        if (uid == "" || !questMap) return false;
        EnsureSaveState();

        if (!WritePlayer(uid, questMap))
            return false;

        s_lastSavedSignatures.Set(uid, BuildQuestMapSignature(questMap));
        QT_Perf.Log("Player saved successfully uid=" + uid + " quests=" + questMap.Count().ToString());
        return true;
    }

    static void FlushAll()
    {
        EnsureSaveState();
    }

    private static bool WritePlayer(string uid, map<string, ref QT_PlayerQuestState> questMap)
    {
        if (!FileExist(SAVE_DIR)) MakeDirectory(SAVE_DIR);

        ref QT_SavedPlayerData saveData = new QT_SavedPlayerData();
        saveData.playerUID = uid;

        foreach (string questId, QT_PlayerQuestState qs : questMap)
        {
            ref QT_SavedQuestState saved = new QT_SavedQuestState();
            saved.questId            = questId;
            saved.state              = qs.state;
            saved.completedTimestamp = qs.completedTimestamp;

            foreach (int progress : qs.objectiveProgress)
            {
                ref QT_SavedObjective so = new QT_SavedObjective();
                so.currentAmount = progress;
                saved.objectives.Insert(so);
            }
            saveData.questStates.Insert(saved);
        }

        string filePath = SAVE_DIR + uid + ".json";
        string tmpPath = filePath + ".tmp";
        string json = "";
        JsonSerializer ser = new JsonSerializer();
        bool ok = ser.WriteToString(saveData, false, json);
        if (!ok || json == "" || json.Length() < 5)
        {
            Print("[QuestTrader] SAVE FAILED for: " + uid + " ok=" + ok + " len=" + json.Length());
            return false;
        }

        FileHandle fh = OpenFile(tmpPath, FileMode.WRITE);
        if (fh == 0)
        {
            Print("[QuestTrader] Cannot open temp file for write: " + tmpPath);
            return false;
        }
        FPrint(fh, json);
        CloseFile(fh);

        bool copyOk = CopyFile(tmpPath, filePath);
        if (FileExist(tmpPath)) DeleteFile(tmpPath);
        if (!copyOk || !FileExist(filePath))
        {
            Print("[QuestTrader] SAVE FAILED while promoting temp file: " + filePath);
            return false;
        }
        return true;
    }

    // Returns loaded map, or null on failure - avoids "out" for complex types
    static ref map<string, ref QT_PlayerQuestState> LoadPlayer(string uid)
    {
        string filePath = SAVE_DIR + uid + ".json";
        string tmpPath = filePath + ".tmp";
        if (!FileExist(filePath) && FileExist(tmpPath))
            filePath = tmpPath;
        if (!FileExist(filePath)) return null;

        string json = "";
        ref QT_SavedPlayerData saveData = ReadPlayerFile(uid, filePath);
        if (!saveData && filePath != tmpPath && FileExist(tmpPath))
        {
            Print("[QuestTrader] Primary save failed to load, trying temp save for: " + uid);
            saveData = ReadPlayerFile(uid, tmpPath);
        }
        if (!saveData) return null;

        ref map<string, ref QT_PlayerQuestState> questMap = new map<string, ref QT_PlayerQuestState>();
        foreach (QT_SavedQuestState saved : saveData.questStates)
        {
            ref QT_PlayerQuestState qs = new QT_PlayerQuestState();
            qs.questId            = saved.questId;
            qs.state              = saved.state;
            qs.completedTimestamp = saved.completedTimestamp;
            foreach (QT_SavedObjective so : saved.objectives)
                qs.objectiveProgress.Insert(so.currentAmount);
            questMap.Insert(saved.questId, qs);
        }

        return questMap;
    }

    static void DeletePlayer(string uid)
    {
        EnsureSaveState();
        if (s_lastSavedSignatures.Contains(uid)) s_lastSavedSignatures.Remove(uid);

        string filePath = SAVE_DIR + uid + ".json";
        if (FileExist(filePath)) DeleteFile(filePath);
        string tmpPath = filePath + ".tmp";
        if (FileExist(tmpPath)) DeleteFile(tmpPath);
    }

    private static void EnsureSaveState()
    {
        if (!s_lastSavedSignatures)
            s_lastSavedSignatures = new map<string, string>();
    }

    private static ref QT_SavedPlayerData ReadPlayerFile(string uid, string filePath)
    {
        string json = "";
        FileHandle fh = OpenFile(filePath, FileMode.READ);
        if (fh == 0) return null;
        string line;
        while (FGets(fh, line) >= 0)
            json = json + line + "\n";
        CloseFile(fh);

        if (json == "" || json.Length() < 5)
        {
            Print("[QuestTrader] Empty save file for: " + uid);
            return null;
        }

        ref QT_SavedPlayerData saveData = new QT_SavedPlayerData();
        string err;
        JsonSerializer ser = new JsonSerializer();
        if (!ser.ReadFromString(saveData, json, err))
        {
            Print("[QuestTrader] LOAD PARSE FAILED for: " + uid + " - " + err);
            return null;
        }

        return saveData;
    }

    private static string BuildQuestMapSignature(map<string, ref QT_PlayerQuestState> questMap)
    {
        string signature = "";
        if (!questMap) return signature;

        foreach (string questId, QT_PlayerQuestState qs : questMap)
        {
            if (!qs) continue;
            signature = signature + questId + ":";
            signature = signature + qs.state.ToString() + ":";
            signature = signature + qs.completedTimestamp.ToString() + ":";
            if (qs.objectiveProgress)
            {
                foreach (int progress : qs.objectiveProgress)
                    signature = signature + progress.ToString() + ",";
            }
            signature = signature + ";";
        }
        return signature;
    }
}
