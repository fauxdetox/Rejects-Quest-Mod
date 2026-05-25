// ============================================================
//  QuestTrader | QT_Performance.c
//  Optional diagnostics and safe defaults for performance guards.
// ============================================================

class QT_Perf
{
    static bool Enabled()
    {
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg || !cfg.Settings) return false;
        return cfg.Settings.EnableQuestTraderPerformanceLogs || cfg.Settings.debugLogging;
    }

    static float EventDebounceSeconds()
    {
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg || !cfg.Settings) return 3.0;
        if (cfg.Settings.QuestTraderEventDebounceSeconds <= 0) return 3.0;
        return cfg.Settings.QuestTraderEventDebounceSeconds;
    }

    static float SaveDebounceSeconds()
    {
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg || !cfg.Settings) return 10.0;
        if (cfg.Settings.QuestSaveDebounceSeconds > 0) return cfg.Settings.QuestSaveDebounceSeconds;
        if (cfg.Settings.QuestTraderSaveDebounceSeconds <= 0) return 10.0;
        return cfg.Settings.QuestTraderSaveDebounceSeconds;
    }

    static int SaveMaxPlayersPerTick()
    {
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg || !cfg.Settings) return 2;
        if (cfg.Settings.QuestSaveMaxPlayersPerTick <= 0) return 2;
        return cfg.Settings.QuestSaveMaxPlayersPerTick;
    }

    static float FullBackupIntervalSeconds()
    {
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg || !cfg.Settings) return 900.0;
        if (cfg.Settings.QuestFullBackupIntervalSeconds <= 0) return 900.0;
        return cfg.Settings.QuestFullBackupIntervalSeconds;
    }

    static int MaxRpcItemsPerPacket()
    {
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg || !cfg.Settings) return 50;
        if (cfg.Settings.QuestTraderMaxRpcItemsPerPacket <= 0) return 50;
        return cfg.Settings.QuestTraderMaxRpcItemsPerPacket;
    }

    static void Log(string message)
    {
        if (!Enabled()) return;
        Print("[QuestTrader][Perf] " + message);
    }
}
