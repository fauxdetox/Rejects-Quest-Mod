// ============================================================
//  QuestTrader | QT_QuestMenu.c  (v6.0 - Tabbed)
//  Active tab: available + active + completed-ready quests
//  Completed tab: permanently turned-in quests
// ============================================================

class QT_QuestEntryUI
{
    string questId;
    string traderId;
    string title;
    string description;
    int    type;
    int    state;
    string acceptMessage;
    string rewardMessage;
    int    cooldownRemaining;
    string greeting;
    ref array<string> objectiveDescs;
    ref array<int>    objectiveRequired;
    ref array<int>    objectiveProgress;
    ref array<string> rewardDescs;
    ref array<string> prereqTitles;

    void QT_QuestEntryUI()
    {
        objectiveDescs    = new array<string>();
        objectiveRequired = new array<int>();
        objectiveProgress = new array<int>();
        rewardDescs       = new array<string>();
        prereqTitles      = new array<string>();
    }
}

class QT_QuestMenu : UIScriptedMenu
{
    private string m_traderId;
    private string m_traderName;

    string GetTraderName() { return m_traderName; }
    private ref array<ref QT_QuestEntryUI> m_allQuests;   // full list from server
    private ref array<ref QT_QuestEntryUI> m_shownQuests; // currently shown in listbox
    private int    m_selectedIndex;
    private int    m_currentTab;

    private TextListboxWidget   m_questListBox;
    private MultilineTextWidget m_descText;
    private MultilineTextWidget m_detailText;
    private MultilineTextWidget m_rewardsText;
    private ButtonWidget        m_acceptBtn;
    private ButtonWidget        m_completeBtn;
    private ButtonWidget        m_cancelBtn;
    private ButtonWidget        m_closeBtn;
    private ButtonWidget        m_tabActive;
    private ButtonWidget        m_tabNow;
    private ButtonWidget        m_tabDone;
    private TextWidget          m_traderLabel;
    private TextWidget          m_npcNameLabel;
    private float               m_tickTimer;       // accumulates delta time
    private static const float  TICK_INTERVAL = 1.0; // refresh every second
    private static const int    TAB_AVAILABLE = 0;
    private static const int    TAB_NOW       = 1;
    private static const int    TAB_DONE      = 2;
    private static const int    QUEST_LIST_MAX_CHARS = 30;

    void QT_QuestMenu()
    {
        m_allQuests    = new array<ref QT_QuestEntryUI>();
        m_shownQuests  = new array<ref QT_QuestEntryUI>();
        m_selectedIndex   = -1;
        m_currentTab      = TAB_AVAILABLE;
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
        m_tabNow       = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnTabNow"));
        m_tabDone      = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnTabDone"));
        m_npcNameLabel = TextWidget.Cast(layoutRoot.FindAnyWidget("NPCName"));

        ApplyLocalization();
        Print("[QuestTrader] QuestMenu ready.");
        return layoutRoot;
    }

    private void ApplyLocalization()
    {
        QT_L10n.ApplyText(layoutRoot, "title_text", "MENU_TITLE");
        QT_L10n.ApplyText(layoutRoot, "BtnTabActive", "MENU_AVAILABLE");
        QT_L10n.ApplyText(layoutRoot, "BtnTabNow", "MENU_NOW");
        QT_L10n.ApplyText(layoutRoot, "BtnTabDone", "MENU_DONE");
        QT_L10n.ApplyText(layoutRoot, "TextWidget3", "MENU_DESCRIPTION");
        QT_L10n.ApplyText(layoutRoot, "TextWidget1", "MENU_OBJECTIVES");
        QT_L10n.ApplyText(layoutRoot, "TextWidget0", "MENU_REWARDS");
        QT_L10n.ApplyText(layoutRoot, "BtnAccept", "BUTTON_ACCEPT");
        QT_L10n.ApplyText(layoutRoot, "BtnComplete", "BUTTON_COMPLETE");
        QT_L10n.ApplyText(layoutRoot, "BtnCancel", "BUTTON_CANCEL");
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

    override void Update(float timeslice)
    {
        super.Update(timeslice);

        // Tick cooldowns down every second and refresh the selected quest display
        m_tickTimer += timeslice;
        if (m_tickTimer >= TICK_INTERVAL)
        {
            m_tickTimer = 0;

            bool anyChanged = false;
            bool tabNeedsRefresh = false;
            foreach (QT_QuestEntryUI q : m_allQuests)
            {
                if (q.cooldownRemaining > 0)
                {
                    q.cooldownRemaining--;
                    if (q.cooldownRemaining <= 0)
                    {
                        q.cooldownRemaining = 0;
                        q.state = QT_QuestState.AVAILABLE;
                        tabNeedsRefresh = true;
                    }
                    anyChanged = true;
                }
            }

            if (tabNeedsRefresh)
            {
                RefreshTab();
                return;
            }

            if (anyChanged && m_selectedIndex >= 0 && m_selectedIndex < m_shownQuests.Count())
            {
                QT_QuestEntryUI selectedQuest = m_shownQuests[m_selectedIndex];
                if (selectedQuest.cooldownRemaining >= 0)
                    SelectQuest(m_selectedIndex);
            }
        }

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

    void SetQuestData(string traderId, string traderName, array<ref QT_QuestEntryUI> quests)
    {
        m_traderId   = traderId;
        m_traderName = traderName;
        if (m_npcNameLabel) m_npcNameLabel.SetText(traderName);
        m_allQuests.Clear();
        foreach (QT_QuestEntryUI e : quests) m_allQuests.Insert(e);

        // if (m_traderLabel && quests.Count() > 0)
        //     m_traderLabel.SetText(quests[0].greeting);

        // Default to available quests
        m_currentTab = TAB_AVAILABLE;
        RefreshTab();
    }

    private void RefreshTab()
    {
        if (m_tabActive) m_tabActive.SetColor(ARGB(255, 40, 40, 40));
        if (m_tabNow)    m_tabNow.SetColor(ARGB(255, 40, 40, 40));
        if (m_tabDone)   m_tabDone.SetColor(ARGB(255, 40, 40, 40));
        if (m_currentTab == TAB_AVAILABLE && m_tabActive) m_tabActive.SetColor(ARGB(255, 50, 130, 50));
        if (m_currentTab == TAB_NOW && m_tabNow)          m_tabNow.SetColor(ARGB(255, 130, 100, 40));
        if (m_currentTab == TAB_DONE && m_tabDone)        m_tabDone.SetColor(ARGB(255, 50, 80, 130));

        m_shownQuests.Clear();
        if (m_questListBox) m_questListBox.ClearItems();

        foreach (QT_QuestEntryUI q : m_allQuests)
        {
            bool isAvailable = (q.state == QT_QuestState.AVAILABLE && q.cooldownRemaining == 0);
            bool isNow = (q.state == QT_QuestState.ACTIVE || q.state == QT_QuestState.COMPLETED);
            bool isDone = (q.state == QT_QuestState.TURNED_IN || q.state == QT_QuestState.COOLDOWN);

            if (m_currentTab == TAB_AVAILABLE && !isAvailable) continue;
            if (m_currentTab == TAB_NOW && !isNow) continue;
            if (m_currentTab == TAB_DONE && !isDone) continue;

            string prefix = "- ";
            if (q.state == QT_QuestState.ACTIVE)    prefix = "> ";
            if (q.state == QT_QuestState.COMPLETED) prefix = "! ";
            if (q.state == QT_QuestState.COOLDOWN)  prefix = "~ ";
            if (q.state == QT_QuestState.TURNED_IN) prefix = "* ";

            m_shownQuests.Insert(q);
            if (m_questListBox) m_questListBox.AddItem(prefix + FitListText(q.title, QUEST_LIST_MAX_CHARS), null, 0);
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

    private string FitListText(string text, int maxChars)
    {
        if (text.Length() <= maxChars) return text;
        if (maxChars <= 3) return text.Substring(0, maxChars);
        return text.Substring(0, maxChars - 3) + "...";
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

        string stateStr = QT_L10n.T("STATE_AVAILABLE");
        if (e.state == QT_QuestState.ACTIVE)    stateStr = QT_L10n.T("STATE_ACTIVE");
        if (e.state == QT_QuestState.COMPLETED) stateStr = QT_L10n.T("STATE_READY");
        if (e.state == QT_QuestState.TURNED_IN) stateStr = QT_L10n.T("STATE_COMPLETED");
        if (e.state == QT_QuestState.COOLDOWN)  stateStr = QT_L10n.T("STATE_COOLDOWN");

        // Show reward message when quest is ready to turn in or already turned in
        if ((e.state == QT_QuestState.COMPLETED || e.state == QT_QuestState.TURNED_IN) && e.rewardMessage != "")
        {
            if (m_descText) m_descText.SetText(e.title + "\n\n" + e.rewardMessage);
        }
        else
        {
            string descFull = e.title + "\n\n" + e.description;

            // If quest has prerequisites, append with divider
            if (e.prereqTitles && e.prereqTitles.Count() > 0 && e.state == QT_QuestState.AVAILABLE && e.cooldownRemaining == 0)
            {
                string prereqStr = "\n\n--------------------------------\n" + QT_L10n.T("REQUIRES") + ":\n";
                foreach (string pt : e.prereqTitles)
                    prereqStr = prereqStr + "  - " + pt + "\n";
                descFull = descFull + prereqStr;
            }

            if (m_descText) m_descText.SetText(descFull);
        }

        string detail = "";
        bool allCollectItemsReady = (e.type == QT_QuestType.COLLECT && e.state == QT_QuestState.ACTIVE && e.objectiveRequired.Count() > 0);
        bool allDeliveryItemsReady = (e.type == QT_QuestType.DELIVER && e.state == QT_QuestState.ACTIVE && e.objectiveRequired.Count() > 0);
        for (int o = 0; o < e.objectiveDescs.Count(); o++)
        {
            int objectiveProgress = 0;
            int objectiveRequired = 1;
            if (o < e.objectiveProgress.Count()) objectiveProgress = e.objectiveProgress[o];
            if (o < e.objectiveRequired.Count()) objectiveRequired = e.objectiveRequired[o];
            bool done = objectiveProgress >= objectiveRequired;
            if (!done)
            {
                allCollectItemsReady = false;
                allDeliveryItemsReady = false;
            }
            string tick = "[ ] ";
            if (done) tick = "[x] ";
            detail = detail + tick + QT_L10n.ResolveText(e.objectiveDescs[o]);
            if (o < e.objectiveProgress.Count())
                detail = detail + " (" + objectiveProgress + "/" + objectiveRequired + ")";
            if ((e.type == QT_QuestType.COLLECT || e.type == QT_QuestType.DELIVER) && done)
                detail = detail + " - " + QT_L10n.T("OBJECTIVE_READY");
            detail = detail + "\n";
        }
        if (allCollectItemsReady)
            detail = detail + "\n" + QT_L10n.T("OBJECTIVES_ALL_READY") + "\n";
        if (allDeliveryItemsReady)
            detail = detail + "\n" + QT_L10n.T("DELIVERY_ALL_READY") + "\n";

        string rewards = "";
        foreach (string rd : e.rewardDescs)
        {
            rewards += QT_L10n.ResolveText(rd) + "\n";
        }

        if (e.cooldownRemaining > 0)
        {
            int cdSecs = e.cooldownRemaining;
            int cdHours = cdSecs / 3600;
            int cdMins = (cdSecs % 3600) / 60;
            int cdSecsOnly = cdSecs % 60;
            string cdStr = "";
            if (cdHours > 0) cdStr = cdHours.ToString() + "h ";
            if (cdMins > 0 || cdHours > 0) cdStr = cdStr + cdMins.ToString() + "m ";
            cdStr = cdStr + cdSecsOnly.ToString() + "s";

            // Show cooldown with divider at bottom of description
            string availableIn = QT_L10n.T("AVAILABLE_IN") + ": " + cdStr;
            if (m_descText) m_descText.SetText(e.title + "\n\n" + e.description + "\n\n--------------------------------\n" + availableIn);
            if (detail == "")
                detail = availableIn;
            else
                detail = detail + "\n" + availableIn;
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
            // Allow one active/ready quest per origin trader.
            if (canAccept)
            {
                foreach (QT_QuestEntryUI anyQ : m_allQuests)
                {
                    if (anyQ.traderId != e.traderId) continue;
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

            // Delivery quest: enable Turn In only when the required item is present.
            if (!canTurnIn && e.state == QT_QuestState.ACTIVE && e.type == QT_QuestType.DELIVER)
            {
                bool allDeliveryMet = (e.objectiveRequired.Count() > 0);
                for (int di = 0; di < e.objectiveRequired.Count(); di++)
                {
                    int deliveryProg = 0;
                    int deliveryReq = e.objectiveRequired[di];
                    if (di < e.objectiveProgress.Count()) deliveryProg = e.objectiveProgress[di];
                    if (deliveryProg < deliveryReq) { allDeliveryMet = false; break; }
                }
                if (allDeliveryMet) canTurnIn = true;
            }
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
            m_currentTab = TAB_AVAILABLE;
            RefreshTab();
            ClearDetail();
            return true;
        }

        if (w == m_tabNow)
        {
            m_currentTab = TAB_NOW;
            RefreshTab();
            ClearDetail();
            return true;
        }

        if (w == m_tabDone)
        {
            m_currentTab = TAB_DONE;
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

}
