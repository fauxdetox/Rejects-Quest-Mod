// ============================================================
//  QuestTrader | QT_QuestHistory.c  (v2.4)
//  Manual JSON serialisation. JsonSerializer cannot handle
//  nested ref arrays so we build/parse JSON strings directly.
// ============================================================

class QT_HistoryEntry
{
    string questId;
    string questTitle;
    string completedAt;
    int    completedEpoch;
    string rewardSummary;
    int    runNumber;
}

class QT_PlayerHistory
{
    string playerUID;
    string playerName;
    int    totalQuestsCompleted;
    ref array<ref QT_HistoryEntry> entries;

    void QT_PlayerHistory()
    {
        entries              = new array<ref QT_HistoryEntry>();
        totalQuestsCompleted = 0;
    }
}

class QT_QuestHistory
{
    private static ref QT_QuestHistory s_instance;
    private static const string HISTORY_DIR = "$profile:QuestTrader/history/";

    private ref map<string, ref QT_PlayerHistory> m_cache;

    static QT_QuestHistory GetInstance()
    {
        if (!s_instance) s_instance = new QT_QuestHistory();
        return s_instance;
    }

    void QT_QuestHistory()
    {
        m_cache = new map<string, ref QT_PlayerHistory>();
        if (!FileExist(HISTORY_DIR)) MakeDirectory(HISTORY_DIR);
    }

    void RecordCompletion(string uid, string playerName, QT_QuestDef def, int epochTime)
    {
        ref QT_PlayerHistory hist = GetOrLoad(uid, playerName);

        int runNumber = 0;
        foreach (QT_HistoryEntry e : hist.entries)
        {
            if (e.questId == def.id) runNumber++;
        }

        string rewardSummary = "";
        foreach (QT_Reward r : def.rewards)
        {
            if (rewardSummary != "") rewardSummary = rewardSummary + ", ";
            rewardSummary = rewardSummary + r.amount.ToString() + "x " + QT_RPCManager.GetDisplayName(r.itemClassName);
        }

        ref QT_HistoryEntry entry = new QT_HistoryEntry();
        entry.questId        = def.id;
        entry.questTitle     = def.title;
        entry.completedEpoch = epochTime;
        entry.completedAt    = GetTimeStr();
        entry.rewardSummary  = rewardSummary;
        entry.runNumber      = runNumber + 1;

        hist.entries.Insert(entry);
        hist.totalQuestsCompleted++;
        hist.playerName = playerName;

        SaveHistory(uid, hist);
        QT_Logger.GetInstance().Info("HISTORY", "Completed: " + def.title + " run#" + entry.runNumber.ToString(), uid, playerName);
    }

    ref QT_PlayerHistory GetHistory(string uid, string playerName = "")
    {
        return GetOrLoad(uid, playerName);
    }

    array<string> GetLeaderboardLines(int topN = 10)
    {
        return new array<string>();
    }

    private ref QT_PlayerHistory GetOrLoad(string uid, string playerName)
    {
        if (m_cache.Contains(uid)) return m_cache.Get(uid);

        string path = HISTORY_DIR + uid + "_history.json";
        ref QT_PlayerHistory hist = new QT_PlayerHistory();

        if (FileExist(path))
        {
            string json = QT_JsonHelper.ReadFileToString(path);
            if (json != "")
                ParseHistoryJson(json, hist);
        }

        hist.playerUID = uid;
        if (playerName != "") hist.playerName = playerName;
        m_cache.Insert(uid, hist);
        return hist;
    }

    // ----------------------------------------------------------
    //  Manual JSON parser
    //  IndexOf(string) only - no start offset overload in Enforce.
    //  We substring past already-processed content instead.
    // ----------------------------------------------------------

    private void ParseHistoryJson(string json, QT_PlayerHistory hist)
    {
        hist.totalQuestsCompleted = ExtractInt(json, "totalQuestsCompleted");
        hist.playerName           = ExtractStr(json, "playerName");
        hist.playerUID            = ExtractStr(json, "playerUID");

        // Trim to everything after "entries":[
        int arrMarker = json.IndexOf("entries");
        if (arrMarker < 0) return;
        string afterEntries = json.Substring(arrMarker, json.Length() - arrMarker);
        int arrOpen = afterEntries.IndexOf("[");
        if (arrOpen < 0) return;
        // Work inside the array content
        string arrContent = afterEntries.Substring(arrOpen + 1, afterEntries.Length() - arrOpen - 1);

        int pos = 0;
        int contentLen = arrContent.Length();

        while (pos < contentLen)
        {
            string ch = arrContent.Substring(pos, 1);
            if (ch == "]") break;
            if (ch != "{") { pos++; continue; }

            // Find matching closing brace
            int depth = 1;
            int objStart = pos;
            pos++;
            while (pos < contentLen && depth > 0)
            {
                string c = arrContent.Substring(pos, 1);
                if (c == "{") depth++;
                else if (c == "}") depth--;
                pos++;
            }
            string objJson = arrContent.Substring(objStart, pos - objStart);

            ref QT_HistoryEntry e = new QT_HistoryEntry();
            e.questId        = ExtractStr(objJson, "questId");
            e.questTitle     = ExtractStr(objJson, "questTitle");
            e.completedAt    = ExtractStr(objJson, "completedAt");
            e.completedEpoch = ExtractInt(objJson, "completedEpoch");
            e.rewardSummary  = ExtractStr(objJson, "rewardSummary");
            e.runNumber      = ExtractInt(objJson, "runNumber");
            hist.entries.Insert(e);
        }

        hist.totalQuestsCompleted = hist.entries.Count();
    }

    // Search for: key + QUOTE + : + QUOTE  then read until next QUOTE.
    // QUOTE is the literal double-quote char embedded via string literal.
    private string ExtractStr(string json, string key)
    {
        string QUOTE = "\"";
        string search = key + QUOTE + ":" + QUOTE;
        int idx = json.IndexOf(search);
        if (idx < 0) return "";
        int start = idx + search.Length();
        int end = start;
        int len = json.Length();
        while (end < len)
        {
            if (json.Substring(end, 1) == QUOTE) break;
            end++;
        }
        return json.Substring(start, end - start);
    }

    private int ExtractInt(string json, string key)
    {
        string QUOTE = "\"";
        string search = key + QUOTE + ":";
        int idx = json.IndexOf(search);
        if (idx < 0) return 0;
        int start = idx + search.Length();
        int end = start;
        int len = json.Length();
        while (end < len)
        {
            string c = json.Substring(end, 1);
            if (c == "," || c == "}" || c == "]" || c == " ") break;
            end++;
        }
        return json.Substring(start, end - start).ToInt();
    }

    // ----------------------------------------------------------
    //  Manual JSON serialiser
    // ----------------------------------------------------------

    private void SaveHistory(string uid, QT_PlayerHistory hist)
    {
        if (!FileExist(HISTORY_DIR)) MakeDirectory(HISTORY_DIR);

        string QUOTE = "\"";
        string json = "{";
        json += QUOTE + "playerUID" + QUOTE + ":" + QUOTE + hist.playerUID + QUOTE + ",";
        json += QUOTE + "playerName" + QUOTE + ":" + QUOTE + hist.playerName + QUOTE + ",";
        json += QUOTE + "totalQuestsCompleted" + QUOTE + ":" + hist.totalQuestsCompleted.ToString() + ",";
        json += QUOTE + "entries" + QUOTE + ":[";

        for (int i = 0; i < hist.entries.Count(); i++)
        {
            QT_HistoryEntry e = hist.entries[i];
            if (i > 0) json += ",";
            json += "{";
            json += QUOTE + "questId" + QUOTE + ":" + QUOTE + e.questId + QUOTE + ",";
            json += QUOTE + "questTitle" + QUOTE + ":" + QUOTE + e.questTitle + QUOTE + ",";
            json += QUOTE + "completedAt" + QUOTE + ":" + QUOTE + e.completedAt + QUOTE + ",";
            json += QUOTE + "completedEpoch" + QUOTE + ":" + e.completedEpoch.ToString() + ",";
            json += QUOTE + "rewardSummary" + QUOTE + ":" + QUOTE + e.rewardSummary + QUOTE + ",";
            json += QUOTE + "runNumber" + QUOTE + ":" + e.runNumber.ToString();
            json += "}";
        }
        json += "]}";

        string path = HISTORY_DIR + uid + "_history.json";
        FileHandle fh = OpenFile(path, FileMode.WRITE);
        if (fh == 0)
        {
            Print("[QuestTrader] ERROR - cannot write history: " + path);
            return;
        }
        FPrint(fh, json);
        CloseFile(fh);
        Print("[QuestTrader] Saved: " + path + " (" + json.Length().ToString() + " bytes)");
    }

    private string PadTwo(int v)
    {
        if (v < 10) return "0" + v.ToString();
        return v.ToString();
    }

    private string GetTimeStr()
    {
        int yr, mo, dy, hr, mn, sc;
        GetYearMonthDay(yr, mo, dy);
        GetHourMinuteSecond(hr, mn, sc);
        return yr.ToString() + "-" + PadTwo(mo) + "-" + PadTwo(dy) + " " + PadTwo(hr) + ":" + PadTwo(mn) + ":" + PadTwo(sc);
    }
}
