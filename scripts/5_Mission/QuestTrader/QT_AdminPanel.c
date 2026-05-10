// ============================================================
//  QuestTrader | QT_AdminPanel.c  (v1.3 - single-line calls)
// ============================================================

class QT_AdminPlayerEntry
{
    string uid;
    string name;
    int    activeQuests;
    int    totalCompleted;
}

class QT_AdminPanel : UIScriptedMenu
{
    private ref array<ref QT_AdminPlayerEntry> m_players;
    private ref array<string>                  m_logLines;
    private int m_selectedPlayer;

    private TextListboxWidget   m_playerList;
    private MultilineTextWidget m_playerDetail;
    private MultilineTextWidget m_logText;
    private ButtonWidget        m_btnCompleteQuest;
    private ButtonWidget        m_btnResetQuest;
    private ButtonWidget        m_btnWipePlayer;
    private ButtonWidget        m_btnReloadConfig;
    private ButtonWidget        m_btnRespawnNPCs;
    private ButtonWidget        m_btnRefreshLog;
    private ButtonWidget        m_btnClose;
    private Widget              m_tabPlayers;
    private Widget              m_tabLogs;
    private ButtonWidget        m_tabBtnPlayers;
    private ButtonWidget        m_tabBtnLogs;
    private TextWidget          m_statusLabel;
    private static const int    PLAYER_LIST_MAX_CHARS = 24;

    void QT_AdminPanel()
    {
        m_players        = new array<ref QT_AdminPlayerEntry>();
        m_logLines       = new array<string>();
        m_selectedPlayer = -1;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/AdminPanel.layout");

        if (!layoutRoot)
        {
            Print("[QuestTrader] AdminPanel.layout not found, UI disabled.");
            return null;
        }

        m_playerList      = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("PlayerList"));
        m_playerDetail    = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("PlayerDetail"));
        m_logText         = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("LogText"));
        m_btnCompleteQuest= ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCompleteQuest"));
        m_btnResetQuest   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnResetQuest"));
        m_btnWipePlayer   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnWipePlayer"));
        m_btnReloadConfig = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnReloadConfig"));
        m_btnRespawnNPCs  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnRespawnNPCs"));
        m_btnRefreshLog   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnRefreshLog"));
        m_btnClose        = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));
        m_tabPlayers      = layoutRoot.FindAnyWidget("TabPlayers");
        m_tabLogs         = layoutRoot.FindAnyWidget("TabLogs");
        m_tabBtnPlayers   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("TabBtnPlayers"));
        m_tabBtnLogs      = ButtonWidget.Cast(layoutRoot.FindAnyWidget("TabBtnLogs"));
        m_statusLabel     = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusLabel"));

        ApplyLocalization();
        ShowTab(0);
        QT_RPCManager.RequestAdminData();
        SetStatus(QT_L10n.Key("ADMIN_LOADED"));
        return layoutRoot;
    }

    private void ApplyLocalization()
    {
        QT_L10n.ApplyText(layoutRoot, "AdminTitle", "ADMIN_TITLE");
        QT_L10n.ApplyText(layoutRoot, "TabBtnPlayers", "ADMIN_PLAYERS");
        QT_L10n.ApplyText(layoutRoot, "TabBtnLogs", "ADMIN_LOGS");
        QT_L10n.ApplyText(layoutRoot, "QuestIdLabel", "ADMIN_QUEST_ID");
        QT_L10n.ApplyText(layoutRoot, "BtnReloadConfig", "ADMIN_RELOAD");
        QT_L10n.ApplyText(layoutRoot, "BtnRespawnNPCs", "ADMIN_RESPAWN_NPCS");
        QT_L10n.ApplyText(layoutRoot, "BtnRefreshLog", "ADMIN_REFRESH_LOG");
        QT_L10n.ApplyText(layoutRoot, "BtnCompleteQuest", "ADMIN_COMPLETE_QUEST");
        QT_L10n.ApplyText(layoutRoot, "BtnResetQuest", "ADMIN_RESET_QUEST");
        QT_L10n.ApplyText(layoutRoot, "BtnWipePlayer", "ADMIN_WIPE_PLAYER");
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
        m_players = players;
        m_playerList.ClearItems();
        foreach (QT_AdminPlayerEntry p : players)
            m_playerList.AddItem(FitListText(p.name, PLAYER_LIST_MAX_CHARS) + "  [" + p.activeQuests + " " + QT_L10n.T("ADMIN_ACTIVE") + " / " + p.totalCompleted + " " + QT_L10n.T("ADMIN_COMPLETED") + "]", null, 0);

        // Auto-select first row so buttons work without needing a list click first
        if (players.Count() > 0)
        {
            m_selectedPlayer = 0;
            UpdatePlayerDetail();
        }

        SetStatus(QT_L10n.T("ADMIN_LOADED_PREFIX") + " " + players.Count() + " " + QT_L10n.T("ADMIN_ONLINE_PLAYERS") + ".");
    }

    private string FitListText(string text, int maxChars)
    {
        if (text.Length() <= maxChars) return text;
        if (maxChars <= 3) return text.Substring(0, maxChars);
        return text.Substring(0, maxChars - 3) + "...";
    }

    void SetLogLines(array<string> lines)
    {
        m_logLines = lines;
        string txt = "";
        foreach (string line : lines) txt = txt + line + "\n";
        if (m_logText) m_logText.SetText(txt);
        SetStatus(QT_L10n.T("ADMIN_LOG_REFRESHED") + " (" + lines.Count() + " " + QT_L10n.T("ADMIN_LINES") + ").");
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_btnClose)       { ClosePanelSafely(); return true; }
        if (w == m_tabBtnPlayers)  { ShowTab(0); return true; }
        if (w == m_tabBtnLogs)     { ShowTab(1); QT_RPCManager.RequestAdminLog(); return true; }

        // TextListboxWidget fires OnClick when a row is clicked — update selection here
        if (w == m_playerList)
        {
            int row = m_playerList.GetSelectedRow();
            if (row >= 0) m_selectedPlayer = row;
            UpdatePlayerDetail();
            return false;
        }

        if (w == m_btnRefreshLog)
        {
            QT_RPCManager.RequestAdminLog();
            SetStatus(QT_L10n.Key("ADMIN_REFRESHING_LOG"));
            return true;
        }
        if (w == m_btnReloadConfig)
        {
            QT_RPCManager.SendAdminCommand("RELOAD_CONFIG", "", "");
            SetStatus(QT_L10n.Key("ADMIN_RELOAD_REQUESTED"));
            return true;
        }
        if (w == m_btnRespawnNPCs)
        {
            QT_RPCManager.SendAdminCommand("RESPAWN_NPCS", "", "");
            SetStatus(QT_L10n.Key("ADMIN_RESPAWN_REQUESTED"));
            return true;
        }
        if (w == m_btnResetQuest && m_selectedPlayer >= 0)
        {
            QT_AdminPlayerEntry resetPlayer = m_players[m_selectedPlayer];
            string questId = GetQuestIdFromInput();
            QT_RPCManager.SendAdminCommand("RESET_QUEST", resetPlayer.uid, questId);
            string resetLabel = questId;
            if (resetLabel == "") resetLabel = "(active quest)";
            SetStatus(QT_L10n.T("ADMIN_RESET_DONE") + " " + resetPlayer.name + " [" + resetLabel + "]");
            return true;
        }
        if (w == m_btnCompleteQuest && m_selectedPlayer >= 0)
        {
            QT_AdminPlayerEntry completePlayer = m_players[m_selectedPlayer];
            string completeQuestId = GetQuestIdFromInput();
            QT_RPCManager.SendAdminCommand("COMPLETE_QUEST", completePlayer.uid, completeQuestId);
            string completeLabel = completeQuestId;
            if (completeLabel == "") completeLabel = "(active quest)";
            SetStatus(QT_L10n.T("ADMIN_COMPLETE_DONE") + " " + completePlayer.name + " [" + completeLabel + "]");
            return true;
        }
        if (w == m_btnWipePlayer && m_selectedPlayer >= 0)
        {
            QT_AdminPlayerEntry wipePlayer = m_players[m_selectedPlayer];
            QT_RPCManager.SendAdminCommand("WIPE_PLAYER", wipePlayer.uid, "");
            SetStatus(QT_L10n.T("ADMIN_WIPED_DONE") + " " + wipePlayer.name);
            return true;
        }
        return false;
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        // Also update selection on OnChange in case the engine fires it instead of OnClick
        if (w == m_playerList)
        {
            int row = m_playerList.GetSelectedRow();
            if (row >= 0) m_selectedPlayer = row;
            UpdatePlayerDetail();
        }
        return false;
    }

    private void UpdatePlayerDetail()
    {
        if (m_selectedPlayer < 0 || m_selectedPlayer >= m_players.Count()) return;
        QT_AdminPlayerEntry p = m_players[m_selectedPlayer];
        if (m_playerDetail)
            m_playerDetail.SetText(QT_L10n.T("ADMIN_NAME") + ": " + p.name + "\n" + QT_L10n.T("ADMIN_UID") + ": " + p.uid + "\n" + QT_L10n.T("ADMIN_ACTIVE") + ": " + p.activeQuests.ToString() + "\n" + QT_L10n.T("ADMIN_COMPLETED") + ": " + p.totalCompleted.ToString());
    }

    private void ShowTab(int tab)
    {
        if (m_tabPlayers) m_tabPlayers.Show(tab == 0);
        if (m_tabLogs)    m_tabLogs.Show(tab == 1);
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
}
