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
    private static const int HUD_CHARS_PER_LINE = 64;
    private static const int HUD_TEXT_SIZE_MAX = 12;
    private static const int HUD_TEXT_SIZE_MIN = 9;
    private static const int HUD_VISIBLE_SLOTS = 4;

    private ref array<ref QT_HUDQuestEntry> m_entries;
    private bool m_visible;
    private bool m_initialised;

    private Widget              m_panel;
    private Widget              m_contentLayout;
    private ref array<MultilineTextWidget> m_contentSlots;
    private TextWidget          m_headerText;
    private TextWidget          m_toggleHint;
    private float               m_hintRefreshTimer;

    void QT_QuestHUD()
    {
        m_entries     = new array<ref QT_HUDQuestEntry>();
        m_contentSlots = new array<MultilineTextWidget>();
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

        m_panel         = layoutRoot.FindAnyWidget("HUDPanel");
        m_contentLayout = layoutRoot.FindAnyWidget("HUDContentLayout");
        m_headerText    = TextWidget.Cast(layoutRoot.FindAnyWidget("HUDHeader"));
        m_toggleHint    = TextWidget.Cast(layoutRoot.FindAnyWidget("HUDToggleHint"));

        RegisterContentSlot("HUDContent1");
        RegisterContentSlot("HUDContent2");
        RegisterContentSlot("HUDContent3");
        RegisterContentSlot("HUDContent4");

        if (m_headerText)  m_headerText.SetText(QT_L10n.Key("HUD_TITLE"));
        RefreshInputHints();

        m_initialised = true;
        QT_Refresh();
        return layoutRoot;
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        m_hintRefreshTimer += timeslice;
        if (m_hintRefreshTimer >= 1.0)
        {
            m_hintRefreshTimer = 0;
            RefreshInputHints();
        }
    }

    private void RefreshInputHints()
    {
        if (!m_toggleHint) return;

        string hintText = "[";
        hintText = hintText + QT_Input.GetBoundKeyName(QT_INPUT_TOGGLE_HUD);
        hintText = hintText + "] ";
        hintText = hintText + QT_L10n.T("HUD_TOGGLE_ACTION");
        m_toggleHint.SetText(hintText);
    }

    private void RegisterContentSlot(string widgetName)
    {
        MultilineTextWidget slot = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget(widgetName));
        if (!slot) return;
        slot.SetText("");
        slot.Show(false);
        m_contentSlots.Insert(slot);
    }

    void SetEntries(array<ref QT_HUDQuestEntry> entries)
    {
        if (entries) m_entries = entries;
        else         m_entries = new array<ref QT_HUDQuestEntry>();
        QT_Refresh();
    }

    void QT_Refresh()
    {
        if (!m_initialised || !m_contentLayout || m_contentSlots.Count() == 0) return;
        RefreshInputHints();

        if (!m_visible || m_entries.Count() == 0)
        {
            if (m_panel) m_panel.Show(false);
            HideContentSlots();
            return;
        }

        if (m_panel) m_panel.Show(true);

        RenderContentSlots();
    }

    private void RenderContentSlots()
    {
        HideContentSlots();
        if (m_entries.Count() == 0) return;

        int renderCount = m_entries.Count();
        if (renderCount > m_contentSlots.Count()) renderCount = m_contentSlots.Count();
        if (renderCount > HUD_VISIBLE_SLOTS) renderCount = HUD_VISIBLE_SLOTS;

        for (int i = 0; i < renderCount; i++)
        {
            MultilineTextWidget slot = m_contentSlots[i];
            if (!slot) continue;

            string text = "";
            if (i == renderCount - 1 && m_entries.Count() > renderCount)
            {
                text = BuildMergedEntryText(i);
            }
            else
            {
                QT_HUDQuestEntry entry = m_entries[i];
                text = BuildEntryText(entry);
            }

            int textSize = GetAutoTextSize(text, GetMaxLinesForSlot(i));
            slot.SetTextExactSize(textSize);
            slot.SetText(FitTextToSlot(text, GetMaxLinesForSlot(i), textSize));
            slot.Show(true);
        }
    }

    private void HideContentSlots()
    {
        foreach (MultilineTextWidget slot : m_contentSlots)
        {
            if (!slot) continue;
            slot.SetText("");
            slot.Show(false);
        }
    }

    private int GetMaxLinesForSlot(int slotIndex)
    {
        if (slotIndex == 0) return 6;
        if (slotIndex == 1) return 7;
        if (slotIndex == 2) return 7;
        return 6;
    }

    private int GetAutoTextSize(string text, int baseMaxLines)
    {
        for (int size = HUD_TEXT_SIZE_MAX; size >= HUD_TEXT_SIZE_MIN; size--)
        {
            int charsPerLine = GetCharsPerLineForTextSize(size);
            int maxLines = GetLineCapacityForTextSize(baseMaxLines, size);
            if (CountWrappedLines(text, charsPerLine) <= maxLines)
                return size;
        }

        return HUD_TEXT_SIZE_MIN;
    }

    private int GetCharsPerLineForTextSize(int textSize)
    {
        int chars = HUD_CHARS_PER_LINE;
        if (textSize < HUD_TEXT_SIZE_MAX)
            chars = chars + ((HUD_TEXT_SIZE_MAX - textSize) * 6);
        return chars;
    }

    private int GetLineCapacityForTextSize(int baseMaxLines, int textSize)
    {
        int capacity = baseMaxLines;
        if (textSize < HUD_TEXT_SIZE_MAX)
            capacity = capacity + (HUD_TEXT_SIZE_MAX - textSize);
        return capacity;
    }

    private int CountWrappedLines(string text, int charsPerLine)
    {
        TStringArray sourceLines = new TStringArray();
        text.Split("\n", sourceLines);

        int count = 0;
        foreach (string sourceLine : sourceLines)
        {
            if (sourceLine == "")
            {
                count++;
                continue;
            }

            string remaining = sourceLine;
            while (remaining.Length() > charsPerLine)
            {
                int cut = FindWrapCut(remaining, charsPerLine);
                count++;
                remaining = remaining.Substring(cut, remaining.Length() - cut).Trim();
            }
            count++;
        }
        return count;
    }

    private string FitTextToSlot(string text, int baseMaxLines, int textSize)
    {
        TStringArray sourceLines = new TStringArray();
        text.Split("\n", sourceLines);

        ref array<string> visualLines = new array<string>();
        int charsPerLine = GetCharsPerLineForTextSize(textSize);
        foreach (string sourceLine : sourceLines)
        {
            AddWrappedLine(visualLines, sourceLine, charsPerLine);
        }

        int maxLines = GetLineCapacityForTextSize(baseMaxLines, textSize);
        if (visualLines.Count() <= maxLines)
            return JoinLines(visualLines, visualLines.Count());

        int keepLines = maxLines;
        if (keepLines < 1) keepLines = 1;
        return JoinLines(visualLines, keepLines);
    }

    private void AddWrappedLine(array<string> output, string line, int maxChars)
    {
        if (line == "")
        {
            output.Insert("");
            return;
        }

        string remaining = line;
        while (remaining.Length() > maxChars)
        {
            int cut = FindWrapCut(remaining, maxChars);
            output.Insert(remaining.Substring(0, cut).Trim());
            remaining = remaining.Substring(cut, remaining.Length() - cut).Trim();
        }
        output.Insert(remaining);
    }

    private int FindWrapCut(string text, int maxChars)
    {
        int cut = maxChars;
        for (int i = maxChars; i > 12; i--)
        {
            string ch = text.Substring(i - 1, 1);
            if (ch == " " || ch == "-" || ch == "/" || ch == "|")
                return i;
        }
        return cut;
    }

    private string JoinLines(array<string> lines, int count)
    {
        string text = "";
        int max = count;
        if (max > lines.Count()) max = lines.Count();
        for (int i = 0; i < max; i++)
        {
            if (text != "") text = text + "\n";
            text = text + lines[i];
        }
        return text;
    }

    private string BuildEntryText(QT_HUDQuestEntry entry)
    {
        if (!entry) return "";
        string text = entry.title;
        if (entry.state == QT_QuestState.COMPLETED)
        {
            text = text + " ";
            text = text + QT_L10n.T("HUD_DONE");
        }

        if (!entry.objectiveLines) return text;
        foreach (string line : entry.objectiveLines)
        {
            text = text + "\n";
            text = text + "  ";
            text = text + QT_L10n.ResolveText(line);
        }
        return text;
    }

    private string BuildMergedEntryText(int startIndex)
    {
        string text = "";
        for (int i = startIndex; i < m_entries.Count(); i++)
        {
            if (text != "") text = text + "\n\n";
            text = text + BuildEntryText(m_entries[i]);
        }
        return text;
    }

    void ToggleVisible()
    {
        m_visible = !m_visible;
        QT_Refresh();
    }

    override bool OnKeyPress(Widget w, int x, int y, int key)
    {
        return false;
    }

    bool IsHUDVisible() { return m_visible; }
}
