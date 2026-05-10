// ============================================================
//  QuestTrader | QT_QuestHUD.c
//  Persistent HUD overlay. Null-safe Init in case layout
//  file is not found (gracefully disabled instead of crash).
// ============================================================

class QT_HUDQuestEntry
{
    string title;
    int    state;
    ref array<string> objectiveLines;

    void QT_HUDQuestEntry()
    {
        objectiveLines = new array<string>();
    }
}

class QT_QuestHUD : UIScriptedMenu
{
    private ref array<ref QT_HUDQuestEntry> m_entries;
    private bool m_visible;
    private bool m_initialised;

    private Widget              m_panel;
    private MultilineTextWidget m_contentText;
    private TextWidget          m_headerText;
    private TextWidget          m_toggleHint;

    static const int TOGGLE_KEY = KeyCode.KC_Y;

    void QT_QuestHUD()
    {
        m_entries     = new array<ref QT_HUDQuestEntry>();
        m_visible     = true;
        m_initialised = false;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestHUD.layout");

        // Null guard - layout file may not exist on some client configurations
        if (!layoutRoot)
        {
            Print("[QuestTrader] WARNING - QuestHUD layout not found, HUD disabled.");
            return null;
        }

        m_panel       = layoutRoot.FindAnyWidget("HUDPanel");
        m_contentText = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("HUDContent"));
        m_headerText  = TextWidget.Cast(layoutRoot.FindAnyWidget("HUDHeader"));
        m_toggleHint  = TextWidget.Cast(layoutRoot.FindAnyWidget("HUDToggleHint"));

        if (m_headerText)  m_headerText.SetText(QT_L10n.Key("HUD_TITLE"));
        if (m_toggleHint) m_toggleHint.SetText(QT_L10n.Key("HUD_TOGGLE"));

        m_initialised = true;
        QT_Refresh();
        return layoutRoot;
    }

    void SetEntries(array<ref QT_HUDQuestEntry> entries)
    {
        m_entries = entries;
        QT_Refresh();
    }

    void QT_Refresh()
    {
        if (!m_initialised || !m_contentText) return;

        if (!m_visible || m_entries.Count() == 0)
        {
            if (m_panel) m_panel.Show(false);
            return;
        }

        if (m_panel) m_panel.Show(true);

        string content = "";
        foreach (QT_HUDQuestEntry entry : m_entries)
        {
            string stateStr = "";
            if (entry.state == QT_QuestState.COMPLETED) stateStr = " " + QT_L10n.T("HUD_DONE");
            content = content + entry.title + stateStr + "\n";
            foreach (string line : entry.objectiveLines)
                content = content + "  " + QT_L10n.ResolveText(line) + "\n";
            content = content + "\n";
        }

        m_contentText.SetText(content);
    }

    void ToggleVisible()
    {
        m_visible = !m_visible;
        QT_Refresh();
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        if (key == TOGGLE_KEY)
        {
            ToggleVisible();
            return true;
        }
        return false;
    }

    bool IsHUDVisible() { return m_visible; }
}
