// ============================================================
//  QuestTrader | QT_Journal.c
//  Player field journal — shows completed quest story entries
//  Opens on ' key, scrollable with mouse wheel
// ============================================================

class QT_Journal extends UIScriptedMenu
{
    private MultilineTextWidget m_journalTextLeft;
    private MultilineTextWidget m_journalTextRight;
    private ButtonWidget m_closeBtn;

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestJournal.layout");
        if (!layoutRoot) return layoutRoot;

        m_journalTextLeft  = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("JournalText"));
        m_journalTextRight = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("JournalTextRight"));
        m_closeBtn         = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCloseJournal"));

        ApplyLocalization();

        if (m_journalTextLeft)
            m_journalTextLeft.SetText(QT_L10n.Key("JOURNAL_LOADING"));

        return layoutRoot;
    }

    private void ApplyLocalization()
    {
        QT_L10n.ApplyText(layoutRoot, "JournalTitle", "JOURNAL_TITLE");
        QT_L10n.ApplyText(layoutRoot, "HintText", "JOURNAL_HINT");
    }

    void SetJournalData(array<ref QT_JournalEntry> entries)
    {
        if (!m_journalTextLeft) return;

        if (entries.Count() == 0)
        {
            m_journalTextLeft.SetText(QT_L10n.T("JOURNAL_EMPTY") + "\n\n" + QT_L10n.T("JOURNAL_EMPTY_HINT"));
            if (m_journalTextRight) m_journalTextRight.SetText("");
            return;
        }

        // Build one text block per trader group, then split across pages
        // by approximate line count. Each entry = ~2 lines, each header = ~3 lines.
        // Left page gets roughly half the total line budget, right page the rest.

        // First pass: count total lines to find the split point
        int totalLines = 0;
        string lastTraderCount = "";
        foreach (QT_JournalEntry countEntry : entries)
        {
            if (countEntry.traderName != lastTraderCount)
            {
                totalLines += 3; // header + divider + blank
                lastTraderCount = countEntry.traderName;
            }
            totalLines += 2; // title + rewardMessage
            if (countEntry.rewardMessage != "") totalLines++;
        }
        int splitAt = totalLines / 2;

        // Second pass: build left and right text
        string textLeft = "";
        string textRight = "";
        bool onRight = false;
        int lineCount = 0;
        string lastTrader = "";

        foreach (QT_JournalEntry entry : entries)
        {
            // Check if we need to emit a trader header
            bool newTrader = (entry.traderName != lastTrader);
            int headerLines = 0;
            if (newTrader) headerLines = 3;
            int entryLines = 2;
            if (entry.rewardMessage != "") entryLines++;

            // Switch to right page if we've hit the midpoint
            if (!onRight && (lineCount + headerLines + entryLines) > splitAt)
                onRight = true;

            string traderHeader = "";
            if (newTrader)
            {
                if (lastTrader != "")
                {
                    if (!onRight) textLeft += "\n";
                    else textRight += "\n";
                }
                string th = entry.traderName;
                th.ToUpper();
                traderHeader = "[ " + th + " ]\n--------------------\n\n";
                lastTrader = entry.traderName;
                lineCount += headerLines;
            }

            string entryText = entry.title + "\n";
            if (entry.rewardMessage != "")
                entryText += entry.rewardMessage + "\n";
            entryText += "\n";
            lineCount += entryLines;

            if (!onRight)
            {
                textLeft += traderHeader + entryText;
            }
            else
            {
                // If this is the first entry on the right and it has a header,
                // make sure we don't duplicate the header on the left
                textRight += traderHeader + entryText;
            }
        }

        m_journalTextLeft.SetText(textLeft);
        if (m_journalTextRight) m_journalTextRight.SetText(textRight);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_closeBtn)
        {
            Close();
            return true;
        }
        return false;
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        // Close on apostrophe (39) or Escape
        if (key == KeyCode.KC_APOSTROPHE || key == KeyCode.KC_ESCAPE)
        {
            Close();
            return true;
        }
        return false;
    }

    override void OnHide()
    {
        GetGame().GetMission().PlayerControlEnable(false);
        GetGame().GetInput().ChangeGameFocus(-1);
        super.OnHide();
    }
}

class QT_JournalEntry
{
    string title;
    string traderName;
    string rewardMessage;
}
