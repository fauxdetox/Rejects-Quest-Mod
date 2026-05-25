// ============================================================
//  QuestTrader | QT_AdminPanel.c
//  Admin UI with stable player list, small player summaries and
//  paged quest history loaded on demand.
// ============================================================

class QT_AdminPlayerEntry
{
    string uid;
    string name;
    string steamId;
    int    activeQuests;
    int    totalCompleted;
    string detailText;
}

class QT_AdminHistoryPage
{
    string uid;
    int page;
    int totalPages;
    int totalEntries;
    ref array<string> lines;

    void QT_AdminHistoryPage()
    {
        lines = new array<string>();
    }
}

class QT_AdminPanel : UIScriptedMenu
{
    private ref array<ref QT_AdminPlayerEntry>    m_players;
    private ref array<string>                     m_logLines;
    private ref map<string, ref QT_AdminHistoryPage> m_historyCache;
    private int m_selectedPlayer;

    private TextListboxWidget   m_playerList;
    private MultilineTextWidget m_playerDetail;
    private TextListboxWidget   m_historyList;
    private TextWidget          m_historyPageLabel;
    private TextListboxWidget   m_logList;
    private ButtonWidget        m_btnHistoryPrev;
    private ButtonWidget        m_btnHistoryNext;
    private ButtonWidget        m_btnCompleteQuest;
    private ButtonWidget        m_btnResetQuest;
    private ButtonWidget        m_btnWipePlayer;
    private ButtonWidget        m_btnReloadConfig;
    private ButtonWidget        m_btnRespawnNPCs;
    private ButtonWidget        m_btnRefreshLog;
    private ButtonWidget        m_btnCopySteamID64;
    private ButtonWidget        m_btnClose;
    private Widget              m_tabPlayers;
    private Widget              m_tabLogs;
    private ButtonWidget        m_tabBtnPlayers;
    private ButtonWidget        m_tabBtnLogs;
    private TextWidget          m_statusLabel;
    private bool                m_rebuildingPlayerList;
    private int                 m_nextPlayersRequestTime;
    private int                 m_nextLogRequestTime;
    private int                 m_nextHistoryRequestTime;
    private int                 m_nextCommandTime;
    private string              m_playersSignature;
    private string              m_lastRenderedPlayerUID;
    private string              m_lastRenderedDetail;
    private string              m_currentHistoryUID;
    private int                 m_currentHistoryPage;

    private static const int    PLAYER_LIST_MAX_CHARS = 24;
    private static const int    ADMIN_REQUEST_DEBOUNCE_MS = 900;
    private static const int    ADMIN_HISTORY_DEBOUNCE_MS = 250;
    private static const int    ADMIN_COMMAND_DEBOUNCE_MS = 1200;
    private static const int    LIST_LINE_MAX_CHARS = 132;

    void QT_AdminPanel()
    {
        m_players        = new array<ref QT_AdminPlayerEntry>();
        m_logLines       = new array<string>();
        m_historyCache   = new map<string, ref QT_AdminHistoryPage>();
        m_selectedPlayer = -1;
        m_rebuildingPlayerList = false;
        m_nextPlayersRequestTime = 0;
        m_nextLogRequestTime = 0;
        m_nextHistoryRequestTime = 0;
        m_nextCommandTime = 0;
        m_playersSignature = "";
        m_lastRenderedPlayerUID = "";
        m_lastRenderedDetail = "";
        m_currentHistoryUID = "";
        m_currentHistoryPage = 0;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/AdminPanel.layout");

        if (!layoutRoot)
        {
            Print("[QuestTrader] AdminPanel.layout not found, UI disabled.");
            return null;
        }

        m_playerList       = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("PlayerList"));
        m_playerDetail     = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("PlayerDetail"));
        m_historyList      = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("PlayerHistoryList"));
        m_historyPageLabel = TextWidget.Cast(layoutRoot.FindAnyWidget("HistoryPageLabel"));
        m_logList          = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("LogList"));
        m_btnHistoryPrev   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnHistoryPrev"));
        m_btnHistoryNext   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnHistoryNext"));
        m_btnCompleteQuest = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCompleteQuest"));
        m_btnResetQuest    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnResetQuest"));
        m_btnWipePlayer    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnWipePlayer"));
        m_btnReloadConfig  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnReloadConfig"));
        m_btnRespawnNPCs   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnRespawnNPCs"));
        m_btnRefreshLog    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnRefreshLog"));
        m_btnCopySteamID64 = ButtonWidget.Cast(layoutRoot.FindAnyWidget("CopySteamID64"));
        m_btnClose         = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));
        m_tabPlayers       = layoutRoot.FindAnyWidget("TabPlayers");
        m_tabLogs          = layoutRoot.FindAnyWidget("TabLogs");
        m_tabBtnPlayers    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("TabBtnPlayers"));
        m_tabBtnLogs       = ButtonWidget.Cast(layoutRoot.FindAnyWidget("TabBtnLogs"));
        m_statusLabel      = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusLabel"));

        if (m_playerDetail) m_playerDetail.SetTextExactSize(12);

        if (!m_playerList || !m_playerDetail || !m_historyList)
            Print("[QuestTrader] AdminPanel missing required widgets: PlayerList=" + (m_playerList != null).ToString() + " PlayerDetail=" + (m_playerDetail != null).ToString() + " PlayerHistoryList=" + (m_historyList != null).ToString());

        ApplyLocalization();
        ShowTab(0, false);
        SetStatus(QT_L10n.Key("ADMIN_LOADED"));
        SetActionButtonsEnabled(false);
        ClearHistory(QT_L10n.Key("ADMIN_SELECT_PLAYER"));
        return layoutRoot;
    }

    private void ApplyLocalization()
    {
        QT_L10n.ApplyText(layoutRoot, "AdminTitle", "ADMIN_TITLE");
        QT_L10n.ApplyText(layoutRoot, "TabBtnPlayers", "ADMIN_PLAYERS");
        QT_L10n.ApplyText(layoutRoot, "TabBtnLogs", "ADMIN_LOGS");
        QT_L10n.ApplyText(layoutRoot, "QuestIdLabel", "ADMIN_QUEST_ID_OPTIONAL");
        QT_L10n.ApplyText(layoutRoot, "BtnReloadConfig", "ADMIN_RELOAD");
        QT_L10n.ApplyText(layoutRoot, "BtnRespawnNPCs", "ADMIN_RESPAWN_NPCS");
        QT_L10n.ApplyText(layoutRoot, "BtnRefreshLog", "ADMIN_REFRESH_LOG");
        QT_L10n.ApplyText(layoutRoot, "BtnCompleteQuest", "ADMIN_COMPLETE_QUEST");
        QT_L10n.ApplyText(layoutRoot, "BtnResetQuest", "ADMIN_RESET_QUEST");
        QT_L10n.ApplyText(layoutRoot, "BtnWipePlayer", "ADMIN_WIPE_PLAYER");
        if (m_btnCopySteamID64) m_btnCopySteamID64.SetText(QT_L10n.T("ADMIN_COPY_STEAMID"));
        if (m_btnHistoryPrev) m_btnHistoryPrev.SetText("<");
        if (m_btnHistoryNext) m_btnHistoryNext.SetText(">");
    }

    override void OnShow()
    {
        super.OnShow();
        GetGame().GetInput().ChangeGameFocus(1);
        GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
    }

    override void OnHide()
    {
        GetGame().GetMission().PlayerControlEnable(false);
        GetGame().GetInput().ChangeGameFocus(-1);
        super.OnHide();
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        if (KeyState(KeyCode.KC_ESCAPE) == 1)
            ClosePanelSafely();
    }

    void SetPlayerData(array<ref QT_AdminPlayerEntry> players)
    {
        if (!players)
        {
            SetActionButtonsEnabled(true);
            SetStatus(QT_L10n.Key("ADMIN_PLAYERS_LOAD_FAILED"));
            return;
        }

        SetActionButtonsEnabled(true);

        string selectedUID = GetSelectedUID();
        string newSignature = BuildPlayersSignature(players);
        if (newSignature == m_playersSignature)
        {
            m_players = players;
            RestoreSelectedPlayer(selectedUID);
            UpdateStatusLoaded(players.Count());
            return;
        }
        m_playersSignature = newSignature;
        m_historyCache.Clear();

        m_players = players;
        m_rebuildingPlayerList = true;
        if (m_playerList) m_playerList.ClearItems();

        int selectedIndex = -1;
        int rowIndex = 0;
        foreach (QT_AdminPlayerEntry p : players)
        {
            string row = BuildPlayerRow(p);
            if (m_playerList) m_playerList.AddItem(row, null, 0);
            if (selectedUID != "" && p.uid == selectedUID)
                selectedIndex = rowIndex;
            rowIndex++;
        }

        if (selectedIndex >= 0)
            m_selectedPlayer = selectedIndex;
        else if (players.Count() > 0)
            m_selectedPlayer = 0;
        else
            m_selectedPlayer = -1;

        if (m_playerList && m_selectedPlayer >= 0)
        {
            m_playerList.SelectRow(m_selectedPlayer);
            m_playerList.SetItemColor(m_selectedPlayer, 0, ARGB(255, 210, 60, 60));
        }
        m_rebuildingPlayerList = false;

        if (m_selectedPlayer >= 0 && m_selectedPlayer < m_players.Count())
        {
            EnsureClientDetailText(m_players[m_selectedPlayer]);
            UpdatePlayerDetail(true);
            RequestSelectedHistoryPage(0, false);
            SetActionButtonsEnabled(true);
        }
        else
        {
            ClearPlayerDetail();
            ClearHistory(QT_L10n.Key("ADMIN_NO_ONLINE_PLAYERS"));
            SetActionButtonsEnabled(true);
        }
        UpdateStatusLoaded(players.Count());
    }

    void SetLogLines(array<string> lines)
    {
        SetActionButtonsEnabled(true);
        m_logLines = lines;

        if (m_logList)
        {
            m_logList.ClearItems();
            if (!lines || lines.Count() == 0)
            {
                m_logList.AddItem(QT_L10n.T("ADMIN_NO_LOGS"), null, 0);
            }
            else
            {
                foreach (string line : lines)
                    m_logList.AddItem(ClampListLine(line), null, 0);
            }
        }

        int lineCount = 0;
        if (lines) lineCount = lines.Count();
        string logStatus = QT_L10n.T("ADMIN_LOG_REFRESHED") + " (" + lineCount.ToString() + " " + QT_L10n.T("ADMIN_LINES") + ").";
        SetStatus(logStatus);
    }

    void SetHistoryPage(string uid, int page, int totalPages, int totalEntries, array<string> lines)
    {
        if (uid == "") return;

        if (totalPages <= 0) totalPages = 1;
        if (page < 0) page = 0;
        if (page >= totalPages) page = totalPages - 1;

        ref QT_AdminHistoryPage history = new QT_AdminHistoryPage();
        history.uid = uid;
        history.page = page;
        history.totalPages = totalPages;
        history.totalEntries = totalEntries;
        if (lines)
        {
            foreach (string line : lines)
                history.lines.Insert(ClampListLine(line));
        }

        string key = BuildHistoryCacheKey(uid, page);
        m_historyCache.Set(key, history);

        if (uid == GetSelectedUID())
            RenderHistoryPage(history);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_playerList)
        {
            SelectPlayerRow(m_playerList.GetSelectedRow());
            return true;
        }
        if (w == m_btnHistoryPrev)
        {
            RequestSelectedHistoryPage(m_currentHistoryPage - 1, false);
            return true;
        }
        if (w == m_btnHistoryNext)
        {
            RequestSelectedHistoryPage(m_currentHistoryPage + 1, false);
            return true;
        }
        if (w == m_btnClose)       { ClosePanelSafely(); return true; }
        if (w == m_tabBtnPlayers)  { ShowTab(0, true); return true; }
        if (w == m_tabBtnLogs)     { ShowTab(1, false); RequestAdminLogDebounced(); return true; }
        if (w == m_btnCopySteamID64)
        {
            CopySelectedSteamID64();
            return true;
        }

        if (w == m_btnRefreshLog)
        {
            RequestAdminLogDebounced();
            return true;
        }
        if (w == m_btnReloadConfig)
        {
            return SendAdminCommandDebounced("RELOAD_CONFIG", "", "", QT_L10n.Key("ADMIN_RELOAD_REQUESTED"));
        }
        if (w == m_btnRespawnNPCs)
        {
            return SendAdminCommandDebounced("RESPAWN_NPCS", "", "", QT_L10n.Key("ADMIN_RESPAWN_REQUESTED"));
        }
        if (w == m_btnCompleteQuest && m_selectedPlayer >= 0)
        {
            QT_AdminPlayerEntry completePlayer = m_players[m_selectedPlayer];
            if (!completePlayer || completePlayer.uid == "")
            {
                SetStatus(QT_L10n.T("ADMIN_SELECT_PLAYER"));
                return true;
            }
            string completeQuestId = GetQuestIdFromInput();

            string completeLabel = completeQuestId;
            if (completeLabel == "") completeLabel = QT_L10n.T("STATE_ACTIVE");

            string completeStatus = QT_L10n.T("ADMIN_COMPLETE_DONE") + " " + completePlayer.name + " [" + completeLabel + "]";
            return SendAdminCommandDebounced("COMPLETE_QUEST", completePlayer.uid, completeQuestId, completeStatus);
        }
        if (w == m_btnResetQuest && m_selectedPlayer >= 0)
        {
            QT_AdminPlayerEntry resetPlayer = m_players[m_selectedPlayer];
            if (!resetPlayer || resetPlayer.uid == "")
            {
                SetStatus(QT_L10n.T("ADMIN_SELECT_PLAYER"));
                return true;
            }
            string questId = GetQuestIdFromInput();
            string resetStatus = QT_L10n.T("ADMIN_RESET_DONE") + " " + resetPlayer.name;
            return SendAdminCommandDebounced("RESET_QUEST", resetPlayer.uid, questId, resetStatus);
        }
        if (w == m_btnWipePlayer && m_selectedPlayer >= 0)
        {
            QT_AdminPlayerEntry wipePlayer = m_players[m_selectedPlayer];
            if (!wipePlayer || wipePlayer.uid == "")
            {
                SetStatus(QT_L10n.T("ADMIN_SELECT_PLAYER"));
                return true;
            }
            string wipeStatus = QT_L10n.T("ADMIN_WIPED_DONE") + " " + wipePlayer.name;
            return SendAdminCommandDebounced("WIPE_PLAYER", wipePlayer.uid, "", wipeStatus);
        }
        return false;
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (m_rebuildingPlayerList)
            return false;

        if (w == m_playerList)
            SelectPlayerRow(m_playerList.GetSelectedRow());

        return false;
    }

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (m_rebuildingPlayerList)
            return false;

        if (w == m_playerList)
        {
            SelectPlayerRow(row);
            return true;
        }
        return false;
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (w == m_playerList)
            SelectPlayerRow(m_playerList.GetSelectedRow());

        return false;
    }

    private void SelectPlayerRow(int row)
    {
        if (m_rebuildingPlayerList)
            return;

        if (row < 0 || row >= m_players.Count())
            return;

        if (m_selectedPlayer == row)
            return;

        // Reset previous row to default colour
        if (m_selectedPlayer >= 0 && m_playerList)
            m_playerList.SetItemColor(m_selectedPlayer, 0, ARGB(255, 220, 220, 220));

        m_selectedPlayer = row;

        // Highlight selected row red
        if (m_playerList)
            m_playerList.SetItemColor(m_selectedPlayer, 0, ARGB(255, 210, 60, 60));

        UpdatePlayerDetail(false);
        RequestSelectedHistoryPage(0, false);
        SetActionButtonsEnabled(true);
    }

    private void UpdatePlayerDetail(bool force = false)
    {
        if (m_selectedPlayer < 0 || m_selectedPlayer >= m_players.Count()) return;
        QT_AdminPlayerEntry p = m_players[m_selectedPlayer];

        if (m_playerDetail)
        {
            EnsureClientDetailText(p);
            string detail = QT_L10n.ResolveText(p.detailText);
            if (!force && p.uid == m_lastRenderedPlayerUID && detail == m_lastRenderedDetail)
                return;

            m_lastRenderedPlayerUID = p.uid;
            m_lastRenderedDetail = detail;
            m_playerDetail.SetText(detail);
        }
    }

    private void ClearPlayerDetail()
    {
        m_lastRenderedPlayerUID = "";
        m_lastRenderedDetail = "";
        if (m_playerDetail) m_playerDetail.SetText("");
    }

    private void EnsureClientDetailText(QT_AdminPlayerEntry p)
    {
        if (!p) return;
        if (p.detailText != "") return;

        string text = "";
        text = text + QT_L10n.T("ADMIN_NAME") + ": " + p.name + "\n";
        text = text + QT_L10n.T("ADMIN_UID") + ": " + SplitLongValue(p.uid) + "\n";
        text = text + QT_L10n.T("ADMIN_STEAMID") + ": " + SplitLongValue(p.steamId) + "\n";
        text = text + QT_L10n.T("ADMIN_ACTIVE") + ": " + p.activeQuests.ToString();
        text = text + " | ";
        text = text + QT_L10n.T("ADMIN_COMPLETED") + ": " + p.totalCompleted.ToString();
        text = text + "\n";
        p.detailText = text;
    }

    private void RequestSelectedHistoryPage(int page, bool forceRequest)
    {
        string uid = GetSelectedUID();
        if (uid == "")
        {
            ClearHistory(QT_L10n.Key("ADMIN_SELECT_PLAYER"));
            return;
        }

        if (page < 0) page = 0;

        string key = BuildHistoryCacheKey(uid, page);
        if (!forceRequest && m_historyCache.Contains(key))
        {
            RenderHistoryPage(m_historyCache.Get(key));
            return;
        }

        int now = GetGame().GetTime();
        if (now < m_nextHistoryRequestTime)
            return;

        m_nextHistoryRequestTime = now + ADMIN_HISTORY_DEBOUNCE_MS;
        m_currentHistoryUID = uid;
        m_currentHistoryPage = page;
        if (m_historyList)
        {
            m_historyList.ClearItems();
            m_historyList.AddItem(QT_L10n.T("ADMIN_HISTORY_LOADING"), null, 0);
        }
        if (m_historyPageLabel) m_historyPageLabel.SetText(QT_L10n.Key("ADMIN_HISTORY_TITLE"));
        UpdateHistoryButtons(0, 1);
        QT_RPCManager.RequestAdminHistoryPage(uid, page);
    }

    private void RenderHistoryPage(QT_AdminHistoryPage history)
    {
        if (!history) return;

        m_currentHistoryUID = history.uid;
        m_currentHistoryPage = history.page;

        if (m_historyList)
        {
            m_historyList.ClearItems();
            if (!history.lines || history.lines.Count() == 0)
            {
                m_historyList.AddItem(QT_L10n.T("ADMIN_HISTORY_EMPTY"), null, 0);
            }
            else
            {
                foreach (string line : history.lines)
                    m_historyList.AddItem(ClampListLine(line), null, 0);
            }
        }

        string label = QT_L10n.T("ADMIN_HISTORY_TITLE") + " ";
        label = label + (history.page + 1).ToString();
        label = label + "/";
        label = label + history.totalPages.ToString();
        label = label + " (";
        label = label + history.totalEntries.ToString();
        label = label + ")";
        if (m_historyPageLabel) m_historyPageLabel.SetText(label);

        UpdateHistoryButtons(history.page, history.totalPages);
    }

    private void ClearHistory(string message)
    {
        m_currentHistoryUID = "";
        m_currentHistoryPage = 0;
        if (m_historyList)
        {
            m_historyList.ClearItems();
            if (message != "") m_historyList.AddItem(message, null, 0);
        }
        if (m_historyPageLabel) m_historyPageLabel.SetText(QT_L10n.Key("ADMIN_HISTORY_TITLE"));
        UpdateHistoryButtons(0, 1);
    }

    private void UpdateHistoryButtons(int page, int totalPages)
    {
        bool hasSelection = GetSelectedUID() != "";
        if (m_btnHistoryPrev) m_btnHistoryPrev.Enable(hasSelection && page > 0);
        if (m_btnHistoryNext) m_btnHistoryNext.Enable(hasSelection && (page + 1) < totalPages);
    }

    private void CopySelectedSteamID64()
    {
        if (m_selectedPlayer < 0 || m_selectedPlayer >= m_players.Count())
        {
            SetStatus(QT_L10n.T("ADMIN_SELECT_PLAYER"));
            return;
        }

        QT_AdminPlayerEntry p = m_players[m_selectedPlayer];
        if (!p || p.steamId == "")
        {
            SetStatus(QT_L10n.T("ADMIN_STEAMID_EMPTY"));
            return;
        }

        g_Game.CopyToClipboard(p.steamId);
        SetStatus(QT_L10n.T("ADMIN_STEAMID_COPIED") + ": " + p.steamId);
    }

    private void ShowTab(int tab, bool requestData)
    {
        if (m_tabPlayers) m_tabPlayers.Show(tab == 0);
        if (m_tabLogs)    m_tabLogs.Show(tab == 1);
        if (tab == 0 && requestData) RequestAdminPlayersDebounced();
    }

    private void SetStatus(string msg)
    {
        if (m_statusLabel) m_statusLabel.SetText(msg);
    }

    private void ClosePanelSafely()
    {
        MissionGameplay mg = MissionGameplay.Cast(GetGame().GetMission());
        if (mg)
        {
            mg.QT_CloseAdminPanel();
            return;
        }
        Close();
    }

    private string GetQuestIdFromInput()
    {
        Widget input = layoutRoot.FindAnyWidget("QuestIdInput");
        if (!input) return "";
        EditBoxWidget eb = EditBoxWidget.Cast(input);
        if (!eb) return "";
        return eb.GetText();
    }

    private void RequestAdminPlayersDebounced()
    {
        int now = GetGame().GetTime();
        if (now < m_nextPlayersRequestTime)
            return;

        m_nextPlayersRequestTime = now + ADMIN_REQUEST_DEBOUNCE_MS;
        SetStatus(QT_L10n.Key("ADMIN_LOADED"));
        QT_RPCManager.RequestAdminData();
    }

    private void RequestAdminLogDebounced()
    {
        int now = GetGame().GetTime();
        if (now < m_nextLogRequestTime)
            return;

        m_nextLogRequestTime = now + ADMIN_REQUEST_DEBOUNCE_MS;
        SetStatus(QT_L10n.Key("ADMIN_REFRESHING_LOG"));
        QT_RPCManager.RequestAdminLog();
    }

    private bool SendAdminCommandDebounced(string cmd, string targetUID, string param, string status)
    {
        int now = GetGame().GetTime();
        if (now < m_nextCommandTime)
        {
            SetStatus(QT_L10n.Key("ADMIN_WAIT_PREVIOUS_ACTION"));
            return true;
        }

        m_nextCommandTime = now + ADMIN_COMMAND_DEBOUNCE_MS;
        SetStatus(status);
        QT_RPCManager.SendAdminCommand(cmd, targetUID, param);
        return true;
    }

    private void SetActionButtonsEnabled(bool enabled)
    {
        bool hasSelection = enabled && m_selectedPlayer >= 0;
        if (m_btnCompleteQuest) m_btnCompleteQuest.Enable(hasSelection);
        if (m_btnResetQuest) m_btnResetQuest.Enable(hasSelection);
        if (m_btnWipePlayer) m_btnWipePlayer.Enable(hasSelection);
        if (m_btnReloadConfig) m_btnReloadConfig.Enable(enabled);
        if (m_btnRespawnNPCs) m_btnRespawnNPCs.Enable(enabled);
        if (m_btnRefreshLog) m_btnRefreshLog.Enable(enabled);
        if (m_btnCopySteamID64) m_btnCopySteamID64.Enable(hasSelection);
    }

    private void RestoreSelectedPlayer(string selectedUID)
    {
        if (selectedUID != "")
        {
            for (int i = 0; i < m_players.Count(); i++)
            {
                QT_AdminPlayerEntry entry = m_players[i];
                if (entry && entry.uid == selectedUID)
                {
                    m_selectedPlayer = i;
                    if (m_playerList) m_playerList.SelectRow(i);
                    UpdatePlayerDetail(false);
                    RequestSelectedHistoryPage(m_currentHistoryPage, false);
                    SetActionButtonsEnabled(true);
                    return;
                }
            }
        }

        if (m_selectedPlayer >= 0 && m_selectedPlayer < m_players.Count())
        {
            if (m_playerList)
            {
                m_playerList.SelectRow(m_selectedPlayer);
                m_playerList.SetItemColor(m_selectedPlayer, 0, ARGB(255, 210, 60, 60));
            }
            UpdatePlayerDetail(false);
            RequestSelectedHistoryPage(m_currentHistoryPage, false);
            SetActionButtonsEnabled(true);
            return;
        }

        if (m_players.Count() > 0)
        {
            m_selectedPlayer = 0;
            if (m_playerList)
            {
                m_playerList.SelectRow(0);
                m_playerList.SetItemColor(0, 0, ARGB(255, 210, 60, 60));
            }
            UpdatePlayerDetail(false);
            RequestSelectedHistoryPage(0, false);
            SetActionButtonsEnabled(true);
        }
        else
        {
            m_selectedPlayer = -1;
            ClearPlayerDetail();
            ClearHistory(QT_L10n.Key("ADMIN_NO_ONLINE_PLAYERS"));
            SetActionButtonsEnabled(true);
        }
    }

    private string BuildPlayersSignature(array<ref QT_AdminPlayerEntry> players)
    {
        string signature = "";
        if (!players) return signature;

        foreach (QT_AdminPlayerEntry p : players)
        {
            if (!p) continue;
            signature = signature + p.uid + "|";
            signature = signature + p.name + "|";
            signature = signature + p.steamId + "|";
            signature = signature + p.activeQuests.ToString() + "|";
            signature = signature + p.totalCompleted.ToString() + "\n";
        }
        return signature;
    }

    private string BuildPlayerRow(QT_AdminPlayerEntry p)
    {
        if (!p) return "";

        string row = FitListText(p.name, PLAYER_LIST_MAX_CHARS);
        row = row + "  [";
        row = row + p.activeQuests.ToString();
        row = row + " ";
        row = row + QT_L10n.T("ADMIN_ACTIVE");
        row = row + " / ";
        row = row + p.totalCompleted.ToString();
        row = row + " ";
        row = row + QT_L10n.T("ADMIN_COMPLETED");
        row = row + "]";
        return row;
    }

    private string FitListText(string text, int maxChars)
    {
        if (text.Length() <= maxChars) return text;
        if (maxChars <= 3) return text.Substring(0, maxChars);
        return text.Substring(0, maxChars - 3) + "...";
    }

    private string ClampListLine(string text)
    {
        if (text.Length() <= LIST_LINE_MAX_CHARS) return text;
        return text.Substring(0, LIST_LINE_MAX_CHARS - 3) + "...";
    }

    private string SplitLongValue(string value)
    {
        if (value.Length() <= 28) return value;

        string output = "";
        string remaining = value;
        while (remaining.Length() > 28)
        {
            if (output != "") output = output + "\n     ";
            output = output + remaining.Substring(0, 28);
            remaining = remaining.Substring(28, remaining.Length() - 28);
        }
        if (remaining != "")
        {
            if (output != "") output = output + "\n     ";
            output = output + remaining;
        }
        return output;
    }

    private string GetSelectedUID()
    {
        if (m_selectedPlayer < 0 || m_selectedPlayer >= m_players.Count()) return "";
        QT_AdminPlayerEntry p = m_players[m_selectedPlayer];
        if (!p) return "";
        return p.uid;
    }

    private string BuildHistoryCacheKey(string uid, int page)
    {
        return uid + ":" + page.ToString();
    }

    private void UpdateStatusLoaded(int count)
    {
        if (count <= 0)
        {
            SetStatus("0 " + QT_L10n.T("ADMIN_ONLINE_PLAYERS"));
            if (m_playerDetail) m_playerDetail.SetText(QT_L10n.Key("ADMIN_NO_ONLINE_PLAYERS"));
            return;
        }

        SetStatus(count.ToString() + " " + QT_L10n.T("ADMIN_ONLINE_PLAYERS"));
    }
}
