// ============================================================
//  QuestTrader | QT_JsonHelper.c
//  Thin wrappers around JsonSerializer so we don't have to
//  deal with JsonFileLoader<T> generic static call issues.
//
//  JsonSerializer API (confirmed from DayZ source):
//    ReadFromString(out T data, string json, out string error) -> bool
//    WriteToString(T data, bool prettyPrint, out string json)  -> bool
// ============================================================

class QT_JsonHelper
{
    // Save any serialisable object to a file
    static bool SaveToFile(string path, Class obj)
    {
        // Ensure parent directory exists
        string dir = path.Substring(0, path.LastIndexOf("/") + 1);
        if (dir != "" && !FileExist(dir))
            MakeDirectory(dir);

        string json = "";
        JsonSerializer ser = new JsonSerializer();
        bool ok = ser.WriteToString(obj, false, json);
        if (!ok || json == "")
        {
            Print("[QuestTrader] JSON serialise FAILED for: " + path);
            return false;
        }

        FileHandle fh = OpenFile(path, FileMode.WRITE);
        if (fh == 0)
        {
            Print("[QuestTrader] Cannot open for write: " + path);
            return false;
        }
        FPrint(fh, json);
        CloseFile(fh);
        Print("[QuestTrader] Saved: " + path + " (" + json.Length() + " bytes)");
        return true;
    }

    // Read a file into a string (returns "" on failure)
    static string ReadFileToString(string path)
    {
        if (!FileExist(path)) return "";

        FileHandle fh = OpenFile(path, FileMode.READ);
        if (fh == 0) return "";

        string content;
        string line;
        while (FGets(fh, line) >= 0)
            content = content + line;
        CloseFile(fh);

        // Some Windows editors save UTF-8 JSON with a BOM. Strip a tiny
        // prefix before the first JSON token so JsonSerializer sees "{".
        int firstBrace = content.IndexOf("{");
        int firstBracket = content.IndexOf("[");
        int firstJsonToken = firstBrace;
        if (firstJsonToken < 0 || (firstBracket >= 0 && firstBracket < firstJsonToken))
            firstJsonToken = firstBracket;
        if (firstJsonToken > 0 && firstJsonToken <= 3)
            content = content.Substring(firstJsonToken, content.Length() - firstJsonToken);

        return content;
    }
}
