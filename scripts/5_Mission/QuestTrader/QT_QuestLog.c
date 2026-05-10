// ============================================================
//  QuestTrader | QT_QuestLog.c
//  Client-side quest log window.
//  Shows:  * Completed quest history (received via RPC)
//          * Server leaderboard (top 10)
//  Opened via "Log" button in the main QuestMenu.
// ============================================================

class QT_QuestLogEntry
{
    string questTitle;
    string completedAt;
    string rewardSummary;
    int    runNumber;
}

// ============================================================

class QT_QuestLog : UIScriptedMenu
{
    private ref array<ref QT_QuestLogEntry> m_history;
    private ref array<string>               m_leaderboard;

    // Widget refs
    private TextListboxWidget   m_historyList;
    private MultilineTextWidget m_detailText;
    private MultilineTextWidget m_leaderboardText;
    private ButtonWidget        m_closeBtn;
    private ButtonWidget        m_tabHistory;
    private ButtonWidget        m_tabLeaderboard;
    private Widget              m_historyPane;
    private Widget              m_leaderPane;

    void QT_QuestLog()
    {
        m_history     = new array<ref QT_QuestLogEntry>();
        m_leaderboard = new array<string>();
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestLog.layout");

        if (!layoutRoot)
        {
            Print("[QuestTrader] QuestLog.layout not found, UI disabled.");
            return null;
        }

        m_historyList      = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("HistoryList"));
        m_detailText       = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("HistoryDetail"));
        m_leaderboardText  = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("LeaderboardText"));
        m_closeBtn         = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));
        m_tabHistory       = ButtonWidget.Cast(layoutRoot.FindAnyWidget("TabHistory"));
        m_tabLeaderboard   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("TabLeaderboard"));
        m_historyPane      = layoutRoot.FindAnyWidget("HistoryPane");
        m_leaderPane       = layoutRoot.FindAnyWidget("LeaderPane");

        ApplyLocalization();
        ShowTab(0);  // default: history
        return layoutRoot;
    }

    private void ApplyLocalization()
    {
        QT_L10n.ApplyText(layoutRoot, "LogTitle", "LOG_TITLE");
        QT_L10n.ApplyText(layoutRoot, "TabHistory", "LOG_HISTORY");
        QT_L10n.ApplyText(layoutRoot, "TabLeaderboard", "LOG_LEADERBOARD");
    }

    // --------------------------------------------------------
    //  Populate from RPC data
    // --------------------------------------------------------
    void SetData(array<ref QT_QuestLogEntry> history, array<string> leaderboard)
    {
        m_history     = history;
        m_leaderboard = leaderboard;
        RefreshHistory();
        RefreshLeaderboard();
    }

    // --------------------------------------------------------
    //  Rebuild history list
    // --------------------------------------------------------
    private void RefreshHistory()
    {
        if (!m_historyList) return;
        m_historyList.ClearItems();

        if (m_history.Count() == 0)
        {
            m_historyList.AddItem(QT_L10n.Key("LOG_EMPTY"), null, 0);
            return;
        }

        // Newest first
        for (int i = m_history.Count() - 1; i >= 0; i--)
        {
            QT_QuestLogEntry e = m_history[i];
            string suffix = "";
            if (e.runNumber > 1) suffix = " [x" + e.runNumber.ToString() + "]";
            m_historyList.AddItem("[x] " + e.questTitle + suffix, null, 0);
        }
    }

    private void RefreshLeaderboard()
    {
        if (!m_leaderboardText) return;

        if (m_leaderboard.Count() == 0)
        {
            m_leaderboardText.SetText(QT_L10n.Key("LOG_NO_LEADERBOARD"));
            return;
        }

        string txt = QT_L10n.T("LOG_TOP") + "\n\n";
        foreach (string line : m_leaderboard)
            txt += line + "\n";
        m_leaderboardText.SetText(txt);
    }

    // --------------------------------------------------------
    //  Widget events
    // --------------------------------------------------------
    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_closeBtn)      { Close(); return true; }
        if (w == m_tabHistory)    { ShowTab(0); return true; }
        if (w == m_tabLeaderboard){ ShowTab(1); return true; }
        return false;
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (w == m_historyList)
        {
            int sel = m_historyList.GetSelectedRow();

            // Map index back (list is newest-first, array is oldest-first)
            int arrayIdx = m_history.Count() - 1 - sel;
            if (arrayIdx >= 0 && arrayIdx < m_history.Count())
                ShowHistoryDetail(m_history[arrayIdx]);
        }
        return false;
    }

    private void ShowHistoryDetail(QT_QuestLogEntry e)
    {
        if (!m_detailText) return;
        string txt = e.questTitle + "\n\n" + QT_L10n.T("LOG_COMPLETED") + ": " + e.completedAt + "\n" + QT_L10n.T("LOG_RUN") + " #" + e.runNumber + "\n\n" + QT_L10n.T("LOG_REWARDS_RECEIVED") + ":\n  " + e.rewardSummary;
        m_detailText.SetText(txt);
    }

    private void ShowTab(int tab)
    {
        if (m_historyPane)  m_historyPane.Show(tab == 0);
        if (m_leaderPane)   m_leaderPane.Show(tab == 1);
    }
}
