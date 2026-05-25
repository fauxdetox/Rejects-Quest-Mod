// ============================================================
//  QuestTrader | QT_Logger.c  (v1.3 - all expressions single-line)
// ============================================================

enum QT_LogLevel
{
    DEBUG_L   = 0,
    INFO_L    = 1,
    WARNING_L = 2,
    ERROR_L   = 3
}

class QT_LogEntry
{
    string timestamp;
    string level;
    string category;
    string message;
    string playerUID;
    string playerName;
}

class QT_Logger
{
    private static ref QT_Logger s_instance;

    private static const string LOG_DIR   = "$profile:QuestTrader/logs/";
    private static const int    RING_SIZE = 200;

    private ref array<ref QT_LogEntry> m_ring;
    private int                        m_ringHead;
    private string                     m_logFile;

    static QT_Logger GetInstance()
    {
        if (!s_instance)
            s_instance = new QT_Logger();
        return s_instance;
    }

    void QT_Logger()
    {
        m_ring     = new array<ref QT_LogEntry>();
        m_ringHead = 0;
        if (!FileExist(LOG_DIR))
            MakeDirectory(LOG_DIR);
        m_logFile = LOG_DIR + GetDateStr() + ".log";
        WriteLog(QT_LogLevel.INFO_L, "SYSTEM", "QuestTrader logger started.", "", "");
    }

    void Info (string cat, string msg, string uid = "", string name = "") { WriteLog(QT_LogLevel.INFO_L,    cat, msg, uid, name); }
    void Warn (string cat, string msg, string uid = "", string name = "") { WriteLog(QT_LogLevel.WARNING_L, cat, msg, uid, name); }
    void Error(string cat, string msg, string uid = "", string name = "") { WriteLog(QT_LogLevel.ERROR_L,   cat, msg, uid, name); }
    void Debug(string cat, string msg, string uid = "", string name = "") { WriteLog(QT_LogLevel.DEBUG_L,   cat, msg, uid, name); }

    void WriteLog(QT_LogLevel level, string category, string message, string playerUID, string playerName)
    {
        ref QT_LogEntry e = new QT_LogEntry();
        e.timestamp  = GetTimeStr();
        e.level      = LevelStr(level);
        e.category   = category;
        e.message    = message;
        e.playerUID  = playerUID;
        e.playerName = playerName;

        if (m_ring.Count() < RING_SIZE)
            m_ring.Insert(e);
        else
        {
            m_ring.Set(m_ringHead, e);
            m_ringHead = (m_ringHead + 1) % RING_SIZE;
        }

        string playerPart = "";
        if (playerUID != "")
            playerPart = " [" + playerName + "]";

        string line = "[" + e.timestamp + "]";
        line = line + " [" + e.level + "]";
        line = line + " [" + category + "]";
        line = line + playerPart;
        line = line + " ";
        line = line + message;

        FileHandle fh = OpenFile(m_logFile, FileMode.APPEND);
        if (fh != 0)
        {
            FPrintln(fh, line);
            CloseFile(fh);
        }
        Print("[QuestTrader] " + line);
    }

    array<string> GetRecentLines(int count = 30)
    {
        array<string> result = new array<string>();
        int total = m_ring.Count();
        int start = Math.Max(0, total - count);
        for (int i = start; i < total; i++)
        {
            QT_LogEntry e = m_ring[i];
            string pp = "";
            if (e.playerUID != "")
                pp = " [" + e.playerName + "]";
            string ln = "[" + e.timestamp + "]";
            ln = ln + " [" + e.level + "]";
            ln = ln + " [" + e.category + "]";
            ln = ln + pp;
            ln = ln + " ";
            ln = ln + e.message;
            result.Insert(ln);
        }
        return result;
    }

    private string PadTwo(int v)
    {
        if (v < 10)
            return "0" + v.ToString();
        return v.ToString();
    }

    private string GetTimeStr()
    {
        int yr, mo, dy, hr, mn, sc;
        GetYearMonthDay(yr, mo, dy);
        GetHourMinuteSecond(hr, mn, sc);
        return yr.ToString() + "-" + PadTwo(mo) + "-" + PadTwo(dy) + " " + PadTwo(hr) + ":" + PadTwo(mn) + ":" + PadTwo(sc);
    }

    private string GetDateStr()
    {
        int yr, mo, dy;
        GetYearMonthDay(yr, mo, dy);
        return yr.ToString() + "-" + PadTwo(mo) + "-" + PadTwo(dy);
    }

    private string LevelStr(QT_LogLevel level)
    {
        if (level == QT_LogLevel.WARNING_L) return "WRN";
        if (level == QT_LogLevel.ERROR_L)   return "ERR";
        if (level == QT_LogLevel.DEBUG_L)   return "DBG";
        return "INF";
    }
}
