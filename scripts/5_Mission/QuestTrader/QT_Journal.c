// ============================================================
//  QuestTrader | QT_Journal.c
//  Player field journal — shows completed quest stories with paging.
//  Opens through the remappable QuestTrader journal input.
// ============================================================

class QT_Journal extends UIScriptedMenu
{
    private static const int JOURNAL_PAGE_SIZE = 2;
    private static const int JOURNAL_DESCRIPTION_CHARS_PER_LINE = 56;
    private static const int JOURNAL_DESCRIPTION_LINES_PER_SLOT = 22;
    private static const string QT_JOURNAL_OPEN_SOUND_SET = "QuestTrader_JournalOpen_SoundSet";
    private static const string QT_JOURNAL_PAGE_SOUND_SET = "QuestTrader_JournalPage_SoundSet";
    private static const string QT_JOURNAL_CLOSE_SOUND_SET = "QuestTrader_JournalClose_SoundSet";

    private ButtonWidget        m_closeBtn;
    private ButtonWidget        m_nextPageBtn;
    private ButtonWidget        m_previousPageBtn;
    private TextWidget          m_pageCountText;
    private ref array<MultilineTextWidget> m_titleSlots;
    private ref array<MultilineTextWidget> m_descriptionSlots;
    private ref array<ref QT_JournalEntry> m_entries;
    private int m_currentPage;

    void QT_Journal()
    {
        m_titleSlots = new array<MultilineTextWidget>();
        m_descriptionSlots = new array<MultilineTextWidget>();
        m_entries = new array<ref QT_JournalEntry>();
        m_currentPage = 0;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestJournal.layout");
        if (!layoutRoot) return layoutRoot;

        m_closeBtn    = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnCloseJournal"));
        m_nextPageBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("NextPage"));
        m_previousPageBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("PreviousPage"));
        m_pageCountText = TextWidget.Cast(layoutRoot.FindAnyWidget("PageCount"));
        for (int slot = 1; slot <= JOURNAL_PAGE_SIZE; slot++)
        {
            string slotNumber = slot.ToString();
            RegisterQuestSlot("Quest" + slotNumber + "JournalTitle", "Quest" + slotNumber + "JournalDescription");
        }

        ApplyLocalization();

        // Show loading text while waiting for server data
        ClearQuestSlots();
        SetSlotText(0, QT_L10n.T("JOURNAL_LOADING"), "");

        return layoutRoot;
    }

    override void OnShow()
    {
        super.OnShow();
        GetGame().GetInput().ChangeGameFocus(1);
        GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
        PlayJournalSound(QT_JOURNAL_OPEN_SOUND_SET, "Journal open");
    }

    private void RegisterQuestSlot(string titleWidgetName, string descriptionWidgetName)
    {
        MultilineTextWidget title = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget(titleWidgetName));
        MultilineTextWidget description = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget(descriptionWidgetName));
        if (!title || !description)
        {
            string altTitleName = titleWidgetName;
            string altDescriptionName = descriptionWidgetName;
            altTitleName.Replace("Journal", "Jornal");
            altDescriptionName.Replace("Journal", "Jornal");
            title = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget(altTitleName));
            description = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget(altDescriptionName));
        }
        if (!title || !description) return;

        NormalizeJournalSlotLayout(title, description);
        title.SetText("");
        description.SetText("");
        title.Show(false);
        description.Show(false);
        m_titleSlots.Insert(title);
        m_descriptionSlots.Insert(description);
    }

    private void NormalizeJournalSlotLayout(MultilineTextWidget title, MultilineTextWidget description)
    {
        if (!title || !description) return;

        float titleW;
        float titleH;
        title.GetSize(titleW, titleH);

        Widget descriptionParent = description.GetParent();
        if (descriptionParent == title)
        {
            title.SetSize(titleW, 0.18);
            description.SetPos(0.0, 0.25);
            description.SetSize(1.0, 0.72);
        }

        title.SetTextExactSize(11);
        description.SetTextExactSize(10);
        title.SetColor(ARGB(255, 46, 26, 10));
        description.SetColor(ARGB(255, 30, 18, 8));
    }

    private void ApplyLocalization()
    {
        QT_L10n.ApplyText(layoutRoot, "JournalTitle", "JOURNAL_TITLE");
        TextWidget hint = TextWidget.Cast(layoutRoot.FindAnyWidget("HintText"));
        if (hint)
        {
            string hintText = QT_L10n.T("JOURNAL_HINT_SCROLL");
            hintText = hintText + " [";
            hintText = hintText + QT_Input.GetBoundKeyName(QT_INPUT_OPEN_JOURNAL);
            hintText = hintText + "] ";
            hintText = hintText + QT_L10n.T("JOURNAL_HINT_CLOSE");
            hint.SetText(hintText);
        }
    }

    void SetJournalData(array<ref QT_JournalEntry> entries)
    {
        m_entries = new array<ref QT_JournalEntry>();
        if (entries)
        {
            foreach (QT_JournalEntry entry : entries)
            {
                AddJournalEntryPages(entry);
            }
        }

        m_currentPage = 0;
        RenderCurrentPage();
    }

    private void AddJournalEntryPages(QT_JournalEntry source)
    {
        if (!source) return;

        string title = QT_L10n.ResolveText(source.title);
        string description = source.rewardMessage;
        if (description == "") description = source.description;
        description = QT_L10n.ResolveText(description);

        ref array<string> chunks = new array<string>();
        BuildJournalDescriptionChunks(description, chunks);

        if (chunks.Count() == 0)
        {
            ref QT_JournalEntry emptyEntry = new QT_JournalEntry();
            emptyEntry.title = title;
            emptyEntry.traderName = source.traderName;
            emptyEntry.description = "";
            emptyEntry.rewardMessage = "";
            m_entries.Insert(emptyEntry);
            return;
        }

        int totalParts = chunks.Count();
        for (int part = 0; part < totalParts; part++)
        {
            ref QT_JournalEntry copy = new QT_JournalEntry();
            copy.title = title;
            if (totalParts > 1)
                copy.title = title + " (" + (part + 1).ToString() + "/" + totalParts.ToString() + ")";
            copy.traderName = source.traderName;
            copy.description = chunks[part];
            copy.rewardMessage = copy.description;
            m_entries.Insert(copy);
        }
    }

    private void BuildJournalDescriptionChunks(string description, array<string> output)
    {
        if (!output) return;
        if (description == "") return;

        TStringArray sourceLines = new TStringArray();
        description.Split("\n", sourceLines);

        ref array<string> currentLines = new array<string>();
        foreach (string sourceLine : sourceLines)
        {
            AddWrappedJournalLine(output, currentLines, sourceLine);
        }

        if (currentLines.Count() > 0)
            output.Insert(JoinJournalLines(currentLines, currentLines.Count()));
    }

    private void AddWrappedJournalLine(array<string> output, array<string> currentLines, string line)
    {
        if (!output || !currentLines) return;

        if (line == "")
        {
            AddJournalVisualLine(output, currentLines, "");
            return;
        }

        string remaining = line;
        while (remaining.Length() > JOURNAL_DESCRIPTION_CHARS_PER_LINE)
        {
            int cut = FindJournalWrapCut(remaining, JOURNAL_DESCRIPTION_CHARS_PER_LINE);
            AddJournalVisualLine(output, currentLines, remaining.Substring(0, cut).Trim());
            remaining = remaining.Substring(cut, remaining.Length() - cut).Trim();
        }

        AddJournalVisualLine(output, currentLines, remaining);
    }

    private void AddJournalVisualLine(array<string> output, array<string> currentLines, string line)
    {
        if (currentLines.Count() >= JOURNAL_DESCRIPTION_LINES_PER_SLOT)
        {
            output.Insert(JoinJournalLines(currentLines, currentLines.Count()));
            currentLines.Clear();
        }
        currentLines.Insert(line);
    }

    private int FindJournalWrapCut(string text, int maxChars)
    {
        for (int i = maxChars; i > 14; i--)
        {
            string ch = text.Substring(i - 1, 1);
            if (IsJournalBreakChar(ch))
                return i;
        }
        return maxChars;
    }

    private bool IsJournalBreakChar(string ch)
    {
        if (ch == " ") return true;
        if (ch == "-") return true;
        if (ch == "/") return true;
        if (ch == "|") return true;
        if (ch == ",") return true;
        if (ch == ".") return true;
        if (ch == ";") return true;
        return false;
    }

    private string JoinJournalLines(array<string> lines, int count)
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

    private void RenderCurrentPage()
    {
        ClearQuestSlots();

        if (m_entries.Count() == 0)
        {
            SetSlotText(0, QT_L10n.T("JOURNAL_EMPTY"), QT_L10n.T("JOURNAL_EMPTY_HINT"));
            UpdatePageControls();
            return;
        }

        int totalPages = GetTotalPages();
        if (m_currentPage < 0) m_currentPage = 0;
        if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;

        int startIndex = m_currentPage * JOURNAL_PAGE_SIZE;
        for (int i = 0; i < JOURNAL_PAGE_SIZE; i++)
        {
            int entryIndex = startIndex + i;
            if (entryIndex >= m_entries.Count()) break;

            QT_JournalEntry entry = m_entries[entryIndex];
            if (!entry) continue;

            string title = entry.title;

            string description = entry.rewardMessage;
            if (description == "") description = entry.description;

            SetSlotText(i, "", description);
        }

        UpdatePageControls();
    }

    private int GetTotalPages()
    {
        if (m_entries.Count() == 0) return 1;
        int pages = m_entries.Count() / JOURNAL_PAGE_SIZE;
        if ((m_entries.Count() % JOURNAL_PAGE_SIZE) > 0) pages++;
        if (pages < 1) pages = 1;
        return pages;
    }

    private void SetSlotText(int slotIndex, string title, string description)
    {
        if (slotIndex < 0 || slotIndex >= m_titleSlots.Count()) return;
        if (slotIndex >= m_descriptionSlots.Count()) return;
        m_titleSlots[slotIndex].Show(true);
        m_descriptionSlots[slotIndex].Show(description != "");
        m_titleSlots[slotIndex].SetText(title);
        m_descriptionSlots[slotIndex].SetText(description);
    }

    private void UpdatePageControls()
    {
        int totalPages = GetTotalPages();
        if (m_pageCountText)
        {
            string pageText = (m_currentPage + 1).ToString();
            pageText = pageText + "/";
            pageText = pageText + totalPages.ToString();
            m_pageCountText.SetText(pageText);
        }
        if (m_previousPageBtn) m_previousPageBtn.Enable(m_currentPage > 0);
        if (m_nextPageBtn) m_nextPageBtn.Enable(m_currentPage < totalPages - 1);
    }

    private void ClearQuestSlots()
    {
        foreach (MultilineTextWidget title : m_titleSlots)
        {
            if (!title) continue;
            title.SetText("");
            title.Show(false);
        }
        foreach (MultilineTextWidget description : m_descriptionSlots)
        {
            if (!description) continue;
            description.SetText("");
            description.Show(false);
        }
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == m_closeBtn)
        {
            CloseJournalSafely();
            return true;
        }
        if (w == m_nextPageBtn)
        {
            if (m_currentPage < GetTotalPages() - 1)
            {
                m_currentPage++;
                RenderCurrentPage();
                PlayJournalSound(QT_JOURNAL_PAGE_SOUND_SET, "Journal page");
            }
            return true;
        }
        if (w == m_previousPageBtn)
        {
            if (m_currentPage > 0)
            {
                m_currentPage--;
                RenderCurrentPage();
                PlayJournalSound(QT_JOURNAL_PAGE_SOUND_SET, "Journal page");
            }
            return true;
        }
        return false;
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            CloseJournalSafely();
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

    private void CloseJournalSafely()
    {
        PlayJournalSound(QT_JOURNAL_CLOSE_SOUND_SET, "Journal close");

        MissionGameplay mg = MissionGameplay.Cast(GetGame().GetMission());
        if (mg)
        {
            mg.QT_CloseJournal();
            return;
        }
        Close();
    }

    private void PlayJournalSound(string soundSet, string label)
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;

        SoundParams soundParams = new SoundParams(soundSet);
        if (!soundParams || !soundParams.IsValid())
        {
            Print("[QuestTrader] " + label + " sound failed: invalid SoundSet " + soundSet);
            return;
        }

        EffectSound sound = SEffectManager.PlaySoundOnObject(soundSet, player, 0, 0, false);
        if (sound)
        {
            sound.SetSoundWaveKind(WaveKind.WAVEEFFECTEX);
            sound.SetAutodestroy(true);
        }
    }
}

class QT_JournalEntry
{
    string title;
    string traderName;
    string description;
    string rewardMessage;
}
