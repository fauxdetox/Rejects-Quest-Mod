// ============================================================
//  QuestTrader | QT_QuestMenu.c  (v6.0 - Tabbed)
//  Active tab: available + active + completed-ready quests
//  Completed tab: permanently turned-in quests
// ============================================================

class QT_QuestEntryUI
{
    string questId;
    string title;
    string description;
    int    type;
    int    state;
    string acceptMessage;
    int    cooldownRemaining;
    string greeting;
    ref array<string> objectiveDescs;
    ref array<int>    objectiveRequired;
    ref array<int>    objectiveProgress;
    ref array<string> rewardDescs;

    void QT_QuestEntryUI()
    {
        objectiveDescs    = new array<string>();
        objectiveRequired = new array<int>();
        objectiveProgress = new array<int>();
        rewardDescs       = new array<string>();
    }
}

class QT_QuestMenu : UIScriptedMenu
{
    private string m_traderId;
    private ref array<ref QT_QuestEntryUI> m_allQuests;   // full list from server
    private ref array<ref QT_QuestEntryUI> m_shownQuests; // currently shown in listbox
    private int    m_selectedIndex;
    private bool   m_showingDoneTab;

    private TextListboxWidget   m_questListBox;
    private MultilineTextWidget m_descText;
    private MultilineTextWidget m_detailText;
    private MultilineTextWidget m_rewardsText;
    private ButtonWidget        m_acceptBtn;
    private ButtonWidget        m_completeBtn;
    private ButtonWidget        m_cancelBtn;
    private ButtonWidget        m_closeBtn;
    private ButtonWidget        m_tabActive;
    private ButtonWidget        m_tabDone;
    private TextWidget          m_traderLabel;
    private TextWidget          m_npcNameLabel;

    void QT_QuestMenu()
    {
        m_allQuests    = new array<ref QT_QuestEntryUI>();
        m_shownQuests  = new array<ref QT_QuestEntryUI>();
        m_selectedIndex   = -1;
        m_showingDoneTab  = false;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestMenu.layout");
        if (!layoutRoot)
        {
            Print("[QuestTrader] QuestMenu layout failed.");
            return null;
        }

        // m_traderLabel  = TextWidget.Cast(layoutRoot.FindAnyWidget("TraderName"));
        m_questListBox = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("QuestList"));
        m_descText     = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("DescText"));
        m_detailText   = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("ObjectivesText"));
        m_rewardsText  = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("RewardsText"));
        m_acceptBtn    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnAccept"));
        m_completeBtn  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnComplete"));
        m_cancelBtn    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCancel"));
        m_closeBtn     = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));
        Print("[QuestTrader] Buttons - accept=" + (m_acceptBtn != null) + " turnIn=" + (m_completeBtn != null) + " cancel=" + (m_cancelBtn != null) + " close=" + (m_closeBtn != null));
        m_tabActive    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnTabActive"));
        m_tabDone      = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnTabDone"));
        m_npcNameLabel = TextWidget.Cast(layoutRoot.FindAnyWidget("NPCName"));

        Print("[QuestTrader] QuestMenu ready.");
        return layoutRoot;
    }

    override void OnShow()
    {
        super.OnShow();
        GetGame().GetInput().ChangeGameFocus(1);
        GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);

        // Hide HUD and hotbar while quest menu is open
        Hud hud = Hud.Cast(GetGame().GetMission().GetHud());
        if (hud) hud.Show(false);
    }

    override void OnHide()
    {
        // Restore HUD and hotbar when quest menu closes
        Hud hud = Hud.Cast(GetGame().GetMission().GetHud());
        if (hud) hud.Show(true);

        GetGame().GetMission().PlayerControlEnable(false);
        GetGame().GetInput().ChangeGameFocus(-1);
        super.OnHide();
    }

    void SetQuestData(string traderId, string traderName, array<ref QT_QuestEntryUI> quests)
    {
        m_traderId = traderId;
        if (m_npcNameLabel) m_npcNameLabel.SetText(traderName);
        m_allQuests.Clear();
        foreach (QT_QuestEntryUI e : quests) m_allQuests.Insert(e);

        // if (m_traderLabel && quests.Count() > 0)
        //     m_traderLabel.SetText(quests[0].greeting);

        // Default to Active tab
        m_showingDoneTab = false;
        RefreshTab();
    }

    private void RefreshTab()
    {
        // Highlight active tab
        if (m_tabActive) m_tabActive.SetColor(ARGB(255, 50, 130, 50));
        if (m_tabDone)   m_tabDone.SetColor(ARGB(255, 40, 40, 40));
        if (m_showingDoneTab)
        {
            if (m_tabActive) m_tabActive.SetColor(ARGB(255, 40, 40, 40));
            if (m_tabDone)   m_tabDone.SetColor(ARGB(255, 50, 80, 130));
        }

        m_shownQuests.Clear();
        if (m_questListBox) m_questListBox.ClearItems();

        foreach (QT_QuestEntryUI q : m_allQuests)
        {
            bool isDone = (q.state == QT_QuestState.TURNED_IN);
            if (m_showingDoneTab != isDone) continue;

            string prefix = "- ";
            if (q.state == QT_QuestState.ACTIVE)    prefix = "> ";
            if (q.state == QT_QuestState.COMPLETED) prefix = "! ";
            if (q.state == QT_QuestState.COOLDOWN)  prefix = "~ ";
            if (q.state == QT_QuestState.TURNED_IN) prefix = "* ";

            m_shownQuests.Insert(q);
            if (m_questListBox) m_questListBox.AddItem(prefix + q.title, null, 0);
        }

        m_selectedIndex = -1;
        ClearDetail();
        if (m_shownQuests.Count() > 0) SelectQuest(0);
        UpdateButtons();
    }

    private void ClearDetail()
    {
        if (m_descText)   m_descText.SetText("");
        if (m_detailText) m_detailText.SetText("");
        if (m_rewardsText) m_rewardsText.SetText("");
    }

    private string WordWrap(string text, int maxChars)
    {
        string result = "";
        string remaining = text;
        while (remaining.Length() > 0)
        {
            // Find newline first - preserve existing newlines
            int nlPos = remaining.IndexOf("\n");
            if (nlPos >= 0 && nlPos <= maxChars)
            {
                result = result + remaining.Substring(0, nlPos) + "\n";
                remaining = remaining.Substring(nlPos + 1, remaining.Length() - nlPos - 1);
                continue;
            }
            if (remaining.Length() <= maxChars)
            {
                result = result + remaining;
                break;
            }
            // Find last space before maxChars to break on word boundary
            int breakPos = maxChars;
            int i = maxChars;
            while (i > 0)
            {
                string ch = remaining.Substring(i, 1);
                if (ch == " ") { breakPos = i; break; }
                i--;
            }
            result = result + remaining.Substring(0, breakPos) + "\n";
            remaining = remaining.Substring(breakPos, remaining.Length() - breakPos);
            // Trim leading space on new line
            if (remaining.Length() > 0 && remaining.Substring(0, 1) == " ")
                remaining = remaining.Substring(1, remaining.Length() - 1);
        }
        return result;
    }

    private void CloseMenuSafely()
    {
        MissionGameplay mg = MissionGameplay.Cast(GetGame().GetMission());
        if (mg)
        {
            mg.QT_CloseQuestMenu();
            return;
        }
        Close();
    }

    private void SelectQuest(int idx)
    {
        m_selectedIndex = idx;
        if (idx < 0 || idx >= m_shownQuests.Count()) return;
        QT_QuestEntryUI e = m_shownQuests[idx];

        string stateStr = "Available";
        if (e.state == QT_QuestState.ACTIVE)    stateStr = "Active";
        if (e.state == QT_QuestState.COMPLETED) stateStr = "Ready to turn in!";
        if (e.state == QT_QuestState.TURNED_IN) stateStr = "Completed";
        if (e.state == QT_QuestState.COOLDOWN)  stateStr = "On cooldown";

        if (m_descText) m_descText.SetText(e.description);

        string detail = "";
        for (int o = 0; o < e.objectiveDescs.Count(); o++)
        {
            bool done = e.objectiveProgress[o] >= e.objectiveRequired[o];
            string tick = "[ ] ";
            if (done) tick = "[x] ";
            detail = detail + tick + e.objectiveDescs[o];
            if (e.type == QT_QuestType.KILL)
                detail = detail + " (" + e.objectiveProgress[o] + "/" + e.objectiveRequired[o] + ")";
            detail = detail + "\n";
        }

        string rewards = "";
        foreach (string rd : e.rewardDescs)
        {
            rewards += rd + "\n";
        }
        if (e.cooldownRemaining > 0)
        {
            int cdSecs = e.cooldownRemaining;
            int cdHours = cdSecs / 3600;
            int cdMins = (cdSecs % 3600) / 60;
            string cdStr = "";
            if (cdHours > 0)
            {
                string hStr = cdHours.ToString();
                cdStr = hStr + "h ";
            }
            string mStr = cdMins.ToString();
            cdStr = cdStr + mStr + "m";
            detail = detail + "\n\n Available in: " + cdStr;
        }

        if (m_detailText)
        {
            m_detailText.SetText(detail);
        }

        if (m_rewardsText)
        {
            m_rewardsText.SetText(rewards);
        }

        UpdateButtons();
    }

    private void UpdateButtons()
    {
        bool canAccept = false;
        bool canTurnIn = false;

        if (m_selectedIndex >= 0 && m_selectedIndex < m_shownQuests.Count())
        {
            QT_QuestEntryUI e = m_shownQuests[m_selectedIndex];
            canAccept = (e.state == QT_QuestState.AVAILABLE && e.cooldownRemaining == 0);
            // Disable Accept if any quest is already active across all lists
            if (canAccept)
            {
                foreach (QT_QuestEntryUI anyQ : m_allQuests)
                {
                    if (anyQ.state == QT_QuestState.ACTIVE || anyQ.state == QT_QuestState.COMPLETED)
                    {
                        canAccept = false;
                        break;
                    }
                }
            }
            canTurnIn = (e.state == QT_QuestState.COMPLETED);

            // Collect quest: enable Turn In if all items present
            if (!canTurnIn && e.state == QT_QuestState.ACTIVE && e.type == QT_QuestType.COLLECT)
            {
                bool allMet = true;
                for (int bi = 0; bi < e.objectiveRequired.Count(); bi++)
                {
                    int prog = 0;
                    int req = e.objectiveRequired[bi];
                    if (bi < e.objectiveProgress.Count()) prog = e.objectiveProgress[bi];
                    if (prog < req) { allMet = false; break; }
                }
                if (allMet) canTurnIn = true;
            }

            // Delivery quest: enable Turn In when active (player has the item)
            if (!canTurnIn && e.state == QT_QuestState.ACTIVE && e.type == QT_QuestType.DELIVER)
                canTurnIn = true;
        }

        if (m_acceptBtn) m_acceptBtn.Enable(canAccept);
        if (m_completeBtn) m_completeBtn.Enable(canTurnIn);

        // Cancel only available when quest is active
        bool canCancel = false;
        if (m_selectedIndex >= 0 && m_selectedIndex < m_shownQuests.Count())
        {
            QT_QuestEntryUI cancelEntry = m_shownQuests[m_selectedIndex];
            canCancel = (cancelEntry.state == QT_QuestState.ACTIVE || cancelEntry.state == QT_QuestState.COMPLETED);
        }
        if (m_cancelBtn) m_cancelBtn.Enable(canCancel);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_questListBox)
        {
            int sel = m_questListBox.GetSelectedRow();
            if (sel >= 0 && sel < m_shownQuests.Count()) SelectQuest(sel);
            return true;
        }

        if (w == m_tabActive)
        {
            m_showingDoneTab = false;
            RefreshTab();
            ClearDetail();
            return true;
        }

        if (w == m_tabDone)
        {
            m_showingDoneTab = true;
            RefreshTab();
            return true;
        }

        if (w == m_closeBtn) { CloseMenuSafely(); return true; }

        if (w == m_acceptBtn && m_selectedIndex >= 0)
        {
            QT_QuestEntryUI entry = m_shownQuests[m_selectedIndex];
            QT_RPCManager.RequestAcceptQuest(entry.questId);
            QT_RPCManager.RequestInteractTrader(m_traderId);
            return true;
        }

        if (w == m_completeBtn && m_selectedIndex >= 0)
        {
            QT_QuestEntryUI turnEntry = m_shownQuests[m_selectedIndex];
            QT_RPCManager.RequestTurnIn(turnEntry.questId, m_traderId);
            QT_RPCManager.RequestInteractTrader(m_traderId);
            return true;
        }

        if (w == m_cancelBtn && m_selectedIndex >= 0)
        {
            QT_QuestEntryUI cancelEntry = m_shownQuests[m_selectedIndex];
            QT_RPCManager.RequestCancelQuest(cancelEntry.questId);
            QT_RPCManager.RequestInteractTrader(m_traderId);
            return true;
        }

        return false;
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (w == m_questListBox)
        {
            int sel = m_questListBox.GetSelectedRow();
            if (sel >= 0 && sel < m_shownQuests.Count()) SelectQuest(sel);
        }
        return false;
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (w == m_questListBox)
        {
            int sel = m_questListBox.GetSelectedRow();
            if (sel >= 0 && sel < m_shownQuests.Count()) SelectQuest(sel);
        }
        return false;
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);

        // Update selected item when using arrow up and arrow down and mouse scroll
        if (m_questListBox)
        {
            int sel = m_questListBox.GetSelectedRow();
            if (sel >= 0 && sel < m_shownQuests.Count() && sel != m_selectedIndex)
                SelectQuest(sel);
        }

        if (KeyState(KeyCode.KC_ESCAPE) == 1)
        {
			CloseMenuSafely();
        }
    }
}
