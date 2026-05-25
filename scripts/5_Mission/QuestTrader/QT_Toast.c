// ============================================================
//  QuestTrader | QT_Toast.c  (v1.2 - fixed)
//  Animated toast notifications loaded from a layout file.
//  Avoids programmatic widget creation (which has a different
//  API in Enforce Script) - uses a pre-built layout instead.
//
//  Layout: QuestTrader/gui/layouts/QuestToast.layout
//  Slots:  Toast0 ... Toast3  (4 stacked panels)
// ============================================================

// QT_ToastType is defined in QuestData.c (4_World)

class QT_ToastEntry
{
    string       message;
    int type;
    float        lifetime;
    float        alpha;
    bool         fadingOut;

    void QT_ToastEntry(string msg, int t, float life = 4.0)
    {
        message   = msg;
        type      = t;
        lifetime  = life;
        alpha     = 0.0;
        fadingOut = false;
    }
}

class QT_Toast
{
    private static ref QT_Toast s_instance;
    private static const string READY_SOUND_SET = "QuestTrader_Ready_SoundSet";

    private ref array<ref QT_ToastEntry> m_queue;
    private ref array<ref QT_ToastEntry> m_active;

    private static const int   MAX_VISIBLE   = 4;
    private static const float FADE_IN_TIME  = 0.3;
    private static const float FADE_OUT_TIME = 0.5;

    // Widget slots loaded from layout
    private Widget               m_root;
    private ref array<Widget>         m_panels;
    private ref array<TextWidget>     m_labels;
    private ref array<Widget>         m_bars;   // left accent strip

    static QT_Toast GetInstance()
    {
        if (!s_instance)
            s_instance = new QT_Toast();
        return s_instance;
    }

    void QT_Toast()
    {
        m_queue  = new array<ref QT_ToastEntry>();
        m_active = new array<ref QT_ToastEntry>();
        m_panels = new array<Widget>();
        m_labels = new array<TextWidget>();
        m_bars   = new array<Widget>();
    }

    // --------------------------------------------------------
    //  Init - call after the layout is loaded
    // --------------------------------------------------------
    void Init(Widget layoutRoot)
    {
        m_root = layoutRoot;
        for (int i = 0; i < MAX_VISIBLE; i++)
        {
            Widget panel = layoutRoot.FindAnyWidget("ToastPanel" + i.ToString());
            if (panel) { panel.Show(false); m_panels.Insert(panel); }

            TextWidget lbl = TextWidget.Cast(layoutRoot.FindAnyWidget("ToastText" + i.ToString()));
            if (lbl) m_labels.Insert(lbl);

            Widget bar = layoutRoot.FindAnyWidget("ToastBar" + i.ToString());
            if (bar) m_bars.Insert(bar);
        }
    }

    // --------------------------------------------------------
    //  Push a toast message
    // --------------------------------------------------------
    void Push(string message, int type = 0, float lifetime = 4.0)
    {
        if (IsReadyNotification(message))
            PlayReadySound();

        m_queue.Insert(new QT_ToastEntry(message, type, lifetime));
    }

    private bool IsReadyNotification(string message)
    {
        if (message.IndexOf("#QuestTrader_QUEST_READY_TURN_IN") == 0) return true;
        if (message.IndexOf("$QuestTrader_QUEST_READY_TURN_IN") == 0) return true;
        if (message.IndexOf("#QuestTrader_STATE_READY") == 0) return true;
        if (message.IndexOf("$QuestTrader_STATE_READY") == 0) return true;

        string translated = QT_L10n.ResolveText(message);
        string readyText = QT_L10n.T("QUEST_READY_TURN_IN");
        if (translated == readyText) return true;
        if (translated.IndexOf(readyText) == 0) return true;

        return false;
    }

    private void PlayReadySound()
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;

        SoundParams soundParams = new SoundParams(READY_SOUND_SET);
        if (!soundParams || !soundParams.IsValid())
        {
            Print("[QuestTrader] Ready toast sound failed: invalid SoundSet " + READY_SOUND_SET);
            return;
        }

        EffectSound sound = SEffectManager.PlaySoundOnObject(READY_SOUND_SET, player, 0, 0, false);
        if (sound)
        {
            sound.SetSoundWaveKind(WaveKind.WAVEEFFECTEX);
            sound.SetAutodestroy(true);
        }
    }

    // --------------------------------------------------------
    //  Update - call from MissionGameplay.OnUpdate
    // --------------------------------------------------------
    void Update(float dt)
    {
        // Promote from queue
        while (m_active.Count() < MAX_VISIBLE && m_queue.Count() > 0)
        {
            m_active.Insert(m_queue[0]);
            m_queue.Remove(0);
        }

        // Tick active toasts
        for (int i = m_active.Count() - 1; i >= 0; i--)
        {
            QT_ToastEntry e = m_active[i];

            if (!e.fadingOut)
            {
                if (e.alpha < 1.0)
                    e.alpha = Math.Min(1.0, e.alpha + dt / FADE_IN_TIME);
                e.lifetime -= dt;
                if (e.lifetime <= FADE_OUT_TIME)
                    e.fadingOut = true;
            }
            else
            {
                e.alpha = Math.Max(0.0, e.alpha - dt / FADE_OUT_TIME);
                if (e.alpha <= 0.0)
                {
                    if (i < m_panels.Count()) m_panels[i].Show(false);
                    m_active.Remove(i);
                    continue;
                }
            }

            if (i < m_panels.Count())
                ApplySlot(i, e);
        }

        // Hide unused slots
        for (int j = m_active.Count(); j < m_panels.Count(); j++)
            m_panels[j].Show(false);
    }

    // --------------------------------------------------------
    //  Apply entry to a widget slot
    // --------------------------------------------------------
    private void ApplySlot(int slot, QT_ToastEntry e)
    {
        if (slot >= m_panels.Count()) return;

        Widget panel = m_panels[slot];
        panel.Show(true);
        panel.SetAlpha(e.alpha);

        if (slot < m_labels.Count())
        {
            m_labels[slot].SetText(QT_L10n.ResolveText(e.message));
            m_labels[slot].SetColor(GetTypeColor(e.type));
        }

        if (slot < m_bars.Count())
            m_bars[slot].SetColor(GetTypeColor(e.type));
    }

    // --------------------------------------------------------
    //  Color per toast type (ARGB int for SetColor)
    // --------------------------------------------------------
    private int GetTypeColor(int t)
    {
        // ARGB format: 0xAARRGGBB
        switch (t)
        {
            case QT_ToastType.ACCEPT:   return ARGB(255, 80,  220, 80);
            case QT_ToastType.PROGRESS: return ARGB(255, 255, 220, 50);
            case QT_ToastType.COMPLETE: return ARGB(255, 255, 180, 0);
            case QT_ToastType.REWARD:   return ARGB(255, 80,  200, 255);
            case QT_ToastType.WARNING:  return ARGB(255, 255, 120, 30);
            default:                    return ARGB(255, 210, 210, 210);
        }
        return ARGB(255, 210, 210, 210);
    }
}
