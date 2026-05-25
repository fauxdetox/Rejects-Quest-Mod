// ============================================================
//  QuestTrader | QT_MissionHooks.c  (5_Mission)
//
//  MissionServer valid overrides (confirmed from source):
//    OnInit, OnUpdate, OnMissionStart, OnMissionFinish, OnEvent,
//    OnGameplayDataHandlerLoad, IsServer
//
//  MissionGameplay valid overrides:
//    OnInit, OnUpdate
//
//  OnRPC -> moved to DayZGame (3_Game/QT_DayZGameHook.c)
//  EntityKilled -> moved to EntityAI.EEKilled (4_World/QT_EntityAIHook.c)
// ============================================================

// ============================================================
//  SERVER
// ============================================================
modded class MissionServer
{
    private ref QT_TraderSpawner m_traderSpawner;
    private float m_autosaveTimer = 0;
    private float m_fullBackupTimer = 0;
    private static const float SAVE_QUEUE_INTERVAL = 5.0;
    private bool m_QTProcessingEvent = false;
    private float m_QTLastEventTime = 0;
    private EventType m_QTLastEventType;
    private ref map<string, float> m_QTLastPlayerEventTime;

    override void OnInit()
    {
        super.OnInit();
        if (!GetGame().IsServer()) return;
        QT_Logger.GetInstance();
        QT_QuestHistory.GetInstance();
        QT_QuestManager.GetInstance();
        Print("[QuestTrader] MissionServer OnInit complete.");
    }

    override void OnMissionStart()
    {
        super.OnMissionStart();
        if (!GetGame().IsServer()) return;

        // Initialise marker singleton before spawning so markers are created on spawn
        QT_TraderMarker.GetInstance();

        // Delay NPC spawn by 15 seconds to allow custom map mods (DayZ Editor Loader etc.)
        // to finish placing objects before traders appear
        m_traderSpawner = new QT_TraderSpawner();
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(DelayedSpawnTraders, 15000, false);
        Print("[QuestTrader] OnMissionStart - trader spawn scheduled in 15 seconds.");
    }

    private void DelayedSpawnTraders()
    {
        m_traderSpawner.SpawnAll(QT_QuestManager.GetInstance().GetConfig());
        QT_Logger.GetInstance().Info("SYSTEM", "QuestTrader NPCs spawned.");
        Print("[QuestTrader] OnMissionStart - spawning traders.");
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!GetGame().IsServer()) return;

        m_autosaveTimer += timeslice;
        if (m_autosaveTimer >= SAVE_QUEUE_INTERVAL)
        {
            m_autosaveTimer = 0;
            QT_QuestManager.GetInstance().ProcessSaveQueue();
        }

        m_fullBackupTimer += timeslice;
        if (m_fullBackupTimer >= QT_Perf.FullBackupIntervalSeconds())
        {
            m_fullBackupTimer = 0;
            QT_QuestManager.GetInstance().SaveAll();
        }

        // Check traders are alive every 30s and respawn if dead
        if (m_traderSpawner) m_traderSpawner.CheckAndRespawn(timeslice);
    }

    override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
    {
        super.InvokeOnConnect(player, identity);
        if (!GetGame().IsServer() || !player || !identity) return;

        QT_QuestManager.GetInstance().RegisterOnlinePlayer(player, identity);
    }

    override void OnMissionFinish()
    {
        super.OnMissionFinish();
        if (GetGame().IsServer())
        {
            QT_QuestManager.GetInstance().SaveAll();
            QT_Logger.GetInstance().Info("SYSTEM", "Mission finished - final save done.");
            QT_TraderMarker.GetInstance().DeleteMarker();
        }
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        if (m_QTProcessingEvent && eventTypeId == m_QTLastEventType)
        {
            QT_Perf.Log("OnEvent reentry ignored eventType=" + QT_GetEventTypeName(eventTypeId));
            return;
        }

        int qtEventStart = GetGame().GetTime();
        m_QTProcessingEvent = true;
        m_QTLastEventType = eventTypeId;
        m_QTLastEventTime = qtEventStart / 1000.0;

        super.OnEvent(eventTypeId, params);

        if (!GetGame().IsServer())
        {
            m_QTProcessingEvent = false;
            return;
        }

        QT_Perf.Log("OnEvent received eventType=" + QT_GetEventTypeName(eventTypeId));

        bool qtUsedEvent = false;

        if (eventTypeId == ClientReadyEventTypeID)
        {
            qtUsedEvent = true;
            ClientReadyEventParams readyParams;
            if (Class.CastTo(readyParams, params) && readyParams)
                QT_HandleClientReadyEvent(eventTypeId, readyParams.param1, PlayerBase.Cast(readyParams.param2));
        }
        else if (eventTypeId == ClientNewReadyEventTypeID)
        {
            qtUsedEvent = true;
            ClientNewReadyEventParams newReadyParams;
            if (Class.CastTo(newReadyParams, params) && newReadyParams)
                QT_HandleClientReadyEvent(eventTypeId, newReadyParams.param1, PlayerBase.Cast(newReadyParams.param2));
        }
        else if (eventTypeId == ClientRespawnEventTypeID)
        {
            qtUsedEvent = true;
            ClientRespawnEventParams respawnParams;
            if (Class.CastTo(respawnParams, params) && respawnParams)
                QT_HandleClientReadyEvent(eventTypeId, respawnParams.param1, PlayerBase.Cast(respawnParams.param2));
        }
        else if (eventTypeId == ClientReconnectEventTypeID)
        {
            qtUsedEvent = true;
            ClientReconnectEventParams reconnectParams;
            if (Class.CastTo(reconnectParams, params) && reconnectParams)
                QT_HandleClientReadyEvent(eventTypeId, reconnectParams.param1, PlayerBase.Cast(reconnectParams.param2));
        }
        else if (eventTypeId == ClientDisconnectedEventTypeID)
        {
            qtUsedEvent = true;
            ClientDisconnectedEventParams disconnectParams;
            if (Class.CastTo(disconnectParams, params) && disconnectParams && disconnectParams.param1)
            {
                QT_RPCManager.CancelPendingForPlayer(disconnectParams.param1.GetId());
                QT_QuestManager.GetInstance().OnPlayerDisconnected(disconnectParams.param1.GetId());
            }
        }
        else if (eventTypeId == ChatMessageEventTypeID)
        {
            qtUsedEvent = true;
            QT_HandleQuestChatCommand(params);
        }

        if (!qtUsedEvent)
        {
            m_QTProcessingEvent = false;
            return;
        }

        int qtElapsed = GetGame().GetTime() - qtEventStart;
        QT_Perf.Log("OnEvent processed eventType=" + QT_GetEventTypeName(eventTypeId) + " elapsedMs=" + qtElapsed.ToString());
        m_QTProcessingEvent = false;
    }

    private void QT_HandleClientReadyEvent(EventType eventTypeId, PlayerIdentity identity, PlayerBase player)
    {
        if (!identity && player)
            identity = player.GetIdentity();
        if (!identity) return;

        string playerKey = identity.GetPlainId();
        if (playerKey == "") playerKey = identity.GetId();
        if (playerKey == "") return;

        if (QT_IsPlayerEventDebounced(playerKey, eventTypeId))
            return;

        QT_QuestManager.GetInstance().RegisterOnlinePlayer(player, identity);
        QT_Perf.Log("player ready processed eventType=" + QT_GetEventTypeName(eventTypeId) + " player=" + identity.GetName() + " key=" + playerKey);
    }

    private bool QT_IsPlayerEventDebounced(string playerKey, EventType eventTypeId)
    {
        if (!m_QTLastPlayerEventTime)
            m_QTLastPlayerEventTime = new map<string, float>();

        float now = GetGame().GetTime() / 1000.0;
        float debounceSeconds = QT_Perf.EventDebounceSeconds();
        string debounceKey = playerKey;

        if (m_QTLastPlayerEventTime.Contains(debounceKey))
        {
            float lastTime = m_QTLastPlayerEventTime.Get(debounceKey);
            if (now - lastTime < debounceSeconds)
            {
                QT_Perf.Log("player event ignored by debounce eventType=" + QT_GetEventTypeName(eventTypeId) + " playerKey=" + playerKey);
                return true;
            }
        }

        m_QTLastPlayerEventTime.Set(debounceKey, now);
        return false;
    }

    private string QT_GetEventTypeName(EventType eventTypeId)
    {
        if (eventTypeId == ClientReadyEventTypeID) return "ClientReadyEventTypeID";
        if (eventTypeId == ClientNewReadyEventTypeID) return "ClientNewReadyEventTypeID";
        if (eventTypeId == ClientRespawnEventTypeID) return "ClientRespawnEventTypeID";
        if (eventTypeId == ClientReconnectEventTypeID) return "ClientReconnectEventTypeID";
        if (eventTypeId == ClientDisconnectedEventTypeID) return "ClientDisconnectedEventTypeID";
        if (eventTypeId == ChatMessageEventTypeID) return "ChatMessageEventTypeID";
        return "UnusedEventType";
    }

    private void QT_HandleQuestChatCommand(Param params)
    {
        ChatMessageEventParams chatParams;
        if (!Class.CastTo(chatParams, params)) return;
        if (!chatParams) return;

        // ChatMessageEventParams: param1=channel, param2=senderName, param3=text, param4=to
        string msg = chatParams.param3;
        string msgTrimmed = msg.Trim();
        string lastWord = msgTrimmed;
        for (int ci = msgTrimmed.Length() - 1; ci >= 0; ci--)
        {
            if (msgTrimmed.Get(ci) == " ")
            {
                lastWord = msgTrimmed.Substring(ci + 1, msgTrimmed.Length() - ci - 1);
                break;
            }
        }
        if (lastWord != "/q" && lastWord != "q") return;

        string senderName = chatParams.param2;
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            if (player.GetIdentity().GetName() != senderName) continue;

            string uid = player.GetIdentity().GetId();
            QT_QuestManager mgr = QT_QuestManager.GetInstance();
            auto playerMap = mgr.GetOrCreatePlayerMap(uid);

            bool any = false;
            int questsEvaluated = 0;
            foreach (string questId, ref QT_PlayerQuestState qs : playerMap)
            {
                questsEvaluated++;
                if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED) continue;
                QT_QuestDef def = mgr.GetQuestDef(questId);
                if (!def) continue;

                string stateStr = "#QuestTrader_STATE_ACTIVE";
                if (qs.state == QT_QuestState.COMPLETED) stateStr = "#QuestTrader_STATE_READY";

                QT_RPCManager.SendChatLine(player, "#QuestTrader_CHAT_PREFIX " + def.title + " - " + stateStr);

                // Look up the trader name for turn-in info
                QT_Config cfg = mgr.GetConfig();
                if (def.type == QT_QuestType.DELIVER)
                {
                    // Deliver quests: show destination trader
                    string destName = def.deliveryTraderId;
                    if (cfg)
                    {
                        foreach (QT_TraderDef td : cfg.TraderNPCPositions)
                        {
                            if (td.id == def.deliveryTraderId)
                            {
                                destName = td.name;
                                break;
                            }
                        }
                    }
                    QT_RPCManager.SendChatLine(player, "#QuestTrader_CHAT_DELIVER_TO: " + destName);
                }
                else
                {
                    // Collect/Kill quests: show objectives then turn-in trader
                    for (int o = 0; o < def.objectives.Count(); o++)
                    {
                        QT_Objective obj = def.objectives[o];
                        int prog = 0;
                        if (def.type == QT_QuestType.COLLECT)
                        {
                            // Count items live from inventory - same as quest menu
                            prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                            if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                        }
                        else if (o < qs.objectiveProgress.Count())
                        {
                            prog = qs.objectiveProgress[o];
                        }
                        string tick = "[ ] ";
                        if (prog >= obj.requiredAmount) tick = "[x] ";
                        string objLine = "  ";
                        objLine = objLine + tick;
                        objLine = objLine + QT_RPCManager.BuildObjectiveDisplayText(def, obj);
                        objLine = objLine + " (";
                        objLine = objLine + prog.ToString();
                        objLine = objLine + "/";
                        objLine = objLine + obj.requiredAmount.ToString();
                        objLine = objLine + ")";
                        QT_RPCManager.SendChatLine(player, objLine);
                    }
                    // Show which trader to return to
                    string turnInName = def.traderId;
                    if (cfg)
                    {
                        foreach (QT_TraderDef td2 : cfg.TraderNPCPositions)
                        {
                            if (td2.id == def.traderId)
                            {
                                turnInName = td2.name;
                                break;
                            }
                        }
                    }
                    QT_RPCManager.SendChatLine(player, "#QuestTrader_CHAT_TURN_IN_TO: " + turnInName);
                }
                any = true;
            }
            if (!any)
                QT_RPCManager.SendChatLine(player, "#QuestTrader_CHAT_PREFIX #QuestTrader_CHAT_NO_ACTIVE");
            QT_Perf.Log("chat /q processed player=" + senderName + " questsEvaluated=" + questsEvaluated.ToString());
            break;
        }
    }
}

// ============================================================
//  CLIENT
// ============================================================
modded class MissionGameplay
{
    private static const string QT_COMPLETE_SOUND_SET = "QuestTrader_Complete_SoundSet";
    private static const string QT_READY_SOUND_SET = "QuestTrader_Ready_SoundSet";
    private static const int QT_MAX_PLAYERS_PER_PACKET = 256;
    private static const int QT_MAX_QUESTS_PER_PACKET = 100;
    private static const int QT_MAX_OBJECTIVES_PER_QUEST = 32;
    private static const int QT_MAX_REWARDS_PER_QUEST = 64;
    private static const int QT_MAX_TRADER_POSITIONS = 256;
    private static const int QT_MAX_LOG_LINES = 256;
    private static const int QT_MAX_TEXT_CHARS = 4096;

    // Trader positions received from server on connect
    private ref array<string>  m_qt_traderIds;
    private ref array<vector>  m_qt_traderPositions;
    private float              m_qt_maxDist = 3.5;
    private ref QT_QuestMenu  m_questMenu;
    private ref QT_QuestHUD   m_questHUD;
    private ref QT_QuestLog   m_questLog;
    private ref QT_Journal    m_journal;
    private ref QT_AdminPanel m_adminPanel;
    private int               m_qt_adminPanelBlockUntil = 0;
    private bool              m_qt_questMenuOpenRequested = false;
    private bool              m_qt_adminPanelOpenRequested = false;
    private bool m_qt_initDone = false;
    private Widget            m_toastRoot;
    private string            m_qt_nearbyTraderId = "";
    private bool              m_qt_hintShown = false;
    private TextWidget        m_qt_hintWidget = null;
    private ref map<string, string> m_qt_traderNames    = new map<string, string>();
    private ref map<string, string> m_qt_traderGreetings = new map<string, string>();
    private ref map<string, float>  m_qt_greetingCooldowns = new map<string, float>();
    private float                   m_qt_inputHintTimer = 0;
    private ref array<string>       m_qt_interactQuestIds = new array<string>();
    private ref array<string>       m_qt_interactClassNames = new array<string>();
    private ref array<vector>       m_qt_interactPositions = new array<vector>();
    private ref array<float>        m_qt_interactDistances = new array<float>();
    private float                   m_qt_interactScanTimer = 0;
    private string                  m_qt_nearbyInteractionQuestId = "";
    private string                  m_qt_nearbyInteractionClassName = "";
    private vector                  m_qt_nearbyInteractionPosition = vector.Zero;

    // Recon timer — supports multiple recon quests
    private static const float  RECON_DURATION     = 60.0;
    private float               m_reconTimer       = 0;
    private bool                m_reconActive      = false;
    private string              m_activeReconQuest = ""; // which recon quest is in progress
    private bool                m_recon179Done     = false;
    private bool                m_recon201Done     = false;

    override void OnInit()
    {
        super.OnInit();
        QT_ClientRPCDispatcherBase.s_instance = new QT_ClientRPCDispatcher();
        LoadTraderPositionsFromConfig();

        m_toastRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestToast.layout");
        if (m_toastRoot)
            QT_Toast.GetInstance().Init(m_toastRoot);

        m_questHUD = new QT_QuestHUD();
        m_questHUD.Init();
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(QT_RequestInitialTraderPositions, Math.RandomInt(8000, 15000), false);
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        // Suppress /q and q from appearing in chat
        // GlobalChatPlus prepends channel prefix so we check the last word
        if (eventTypeId == ChatMessageEventTypeID)
        {
            ChatMessageEventParams chatParams;
            if (Class.CastTo(chatParams, params) && chatParams)
            {
                string msg = chatParams.param3.Trim();
                string lastWord = msg;
                for (int i = msg.Length() - 1; i >= 0; i--)
                {
                    if (msg.Get(i) == " ")
                    {
                        lastWord = msg.Substring(i + 1, msg.Length() - i - 1);
                        break;
                    }
                }
                if (lastWord == "/q" || lastWord == "q")
                    return; // swallow - do not call super, message won't display
            }
        }
        super.OnEvent(eventTypeId, params);
    }

    private void LoadTraderPositionsFromConfig()
    {
        m_qt_traderIds       = new array<string>();
        m_qt_traderPositions = new array<vector>();

        // Always create the hint widget - positions will arrive via TRADER_POSITIONS RPC
        Widget hintRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestHint.layout");
        if (hintRoot) m_qt_hintWidget = TextWidget.Cast(hintRoot.FindAnyWidget("QTHint"));
        if (m_qt_hintWidget) m_qt_hintWidget.SetText(QT_GetInteractHint());
        if (m_qt_hintWidget) m_qt_hintWidget.Show(false);

        Print("[QuestTrader] Client: hint widget ready, awaiting trader positions from server.");
    }

    private void QT_RequestInitialTraderPositions()
    {
        if (!GetGame().IsClient()) return;
        QT_RPCManager.RequestTraderPositions();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        QT_Toast.GetInstance().Update(timeslice);
        if (!GetGame().IsClient()) return;

        bool blockQuestTraderShortcut = QT_Input.ShouldBlockQuestTraderShortcut();

        m_qt_inputHintTimer += timeslice;
        if (m_qt_inputHintTimer >= 1.0)
        {
            m_qt_inputHintTimer = 0;
            if (m_qt_hintWidget && m_qt_nearbyTraderId != "" && !m_questMenu && !m_journal && !blockQuestTraderShortcut)
                m_qt_hintWidget.SetText(QT_GetInteractHint());
        }

        if (QT_Input.LocalPress(QT_INPUT_TOGGLE_HUD) && !blockQuestTraderShortcut && m_questHUD && !m_questMenu && !m_adminPanel && !m_questLog && !m_journal)
            m_questHUD.ToggleVisible();

        if (QT_Input.LocalPress(QT_INPUT_OPEN_ADMIN))
        {
            int adminNow = GetGame().GetTime();
            if (m_adminPanel && !blockQuestTraderShortcut)
                QT_CloseAdminPanel();
            else if (adminNow >= m_qt_adminPanelBlockUntil && !blockQuestTraderShortcut && !m_questMenu && !m_questLog && !m_journal)
            {
                m_qt_adminPanelOpenRequested = true;
                QT_RPCManager.RequestAdminData();
            }
        }

        if (QT_Input.LocalPress(QT_INPUT_OPEN_JOURNAL))
        {
            if (m_journal && !blockQuestTraderShortcut)
                QT_CloseJournal();
            else if (!blockQuestTraderShortcut && !m_questMenu && !m_adminPanel && !m_questLog)
                QT_RPCManager.RequestJournal();
        }

        if (m_journal && KeyState(KeyCode.KC_ESCAPE) == 1)
            QT_CloseJournal();

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;
        string closestId = "";
        float closestDist = float.MAX;
        if (m_qt_traderIds && m_qt_traderIds.Count() > 0)
        {
            vector pPos = player.GetPosition();
            for (int ti = 0; ti < m_qt_traderIds.Count(); ti++)
            {
                float d = vector.Distance(pPos, m_qt_traderPositions[ti]);
                if (d <= m_qt_maxDist && d < closestDist)
                {
                    closestDist = d;
                    closestId = m_qt_traderIds[ti];
                }
            }
        }
        // Show hint when proximity changes
        if (closestId != m_qt_nearbyTraderId)
        {
            if (m_qt_hintWidget)
            {
                if (closestId != "") m_qt_hintWidget.SetText(QT_GetInteractHint());
                m_qt_hintWidget.Show(closestId != "" && !m_questMenu && !m_journal && !blockQuestTraderShortcut);
            }

            // Fire greeting in chat when entering a trader's range
            if (closestId != "" && m_qt_traderGreetings.Contains(closestId))
            {
                float now = GetGame().GetTime() / 1000.0;
                float lastGreet = 0;
                if (m_qt_greetingCooldowns.Contains(closestId))
                    lastGreet = m_qt_greetingCooldowns.Get(closestId);

                if (now - lastGreet >= 30.0)
                {
                    string greet = m_qt_traderGreetings.Get(closestId);
                    string tName = m_qt_traderNames.Get(closestId);
                    if (greet != "")
                    {
                        GetGame().Chat(tName + ": " + QT_L10n.ResolveText(greet), "colorAction");
                        m_qt_greetingCooldowns.Set(closestId, now);
                    }
                }
            }
        }
        // Keep hint hidden while menu is open
        if (m_qt_hintWidget && (m_questMenu || m_journal || blockQuestTraderShortcut)) m_qt_hintWidget.Show(false);

        m_qt_nearbyTraderId = closestId;

        UpdateNearbyInteraction(player, timeslice, blockQuestTraderShortcut);

        // Fire interact on F press when near trader and menu not open
        if (m_qt_nearbyTraderId != "" && !m_questMenu)
        {
            if (QT_Input.LocalPress(QT_INPUT_OPEN_QUEST_MENU) && !blockQuestTraderShortcut)
            {
                m_qt_questMenuOpenRequested = true;
                QT_RPCManager.RequestInteractTrader(m_qt_nearbyTraderId);
            }
        }
        else if (m_qt_nearbyInteractionQuestId != "" && !m_questMenu)
        {
            if (QT_Input.LocalPress(QT_INPUT_OPEN_QUEST_MENU) && !blockQuestTraderShortcut)
                QT_RPCManager.RequestObjectInteract(m_qt_nearbyInteractionQuestId, m_qt_nearbyInteractionClassName, m_qt_nearbyInteractionPosition);
        }

        // ── Recon timers ───────────────────────────────────────────
        // quest_179 — Rify Lighthouse
        vector recon179Pos = Vector(13989.0, 32.38, 11221.1);
        // quest_201 — NWAF ATC Tower
        vector recon201Pos = Vector(4627.25, 358.942, 10429.3);

        // Determine which recon quest is currently active (if any)
        string activeRecon = "";
        vector activeReconPos;
        bool reconDone = false;

        if (!m_recon179Done && vector.Distance(player.GetPosition(), recon179Pos) <= 5.0)
        {
            activeRecon = "quest_179";
            activeReconPos = recon179Pos;
            reconDone = m_recon179Done;
        }
        else if (!m_recon201Done && vector.Distance(player.GetPosition(), recon201Pos) <= 5.0)
        {
            activeRecon = "quest_201";
            activeReconPos = recon201Pos;
            reconDone = m_recon201Done;
        }

        // Reset timer if player switched recon zones
        if (activeRecon != "" && m_activeReconQuest != "" && activeRecon != m_activeReconQuest)
        {
            m_reconTimer = 0;
            m_reconActive = false;
        }
        m_activeReconQuest = activeRecon;

        if (activeRecon != "")
        {
            EntityAI heldItem = player.GetItemInHands();
            bool holdingBinos = heldItem && heldItem.GetType() == "Binoculars";
            bool aiming = holdingBinos && GetGame().GetInput().LocalValue("UAFire", false) > 0.5;

            if (holdingBinos && aiming)
            {
                m_reconActive = true;
                m_reconTimer += timeslice;

                int remaining = Math.Max(0, RECON_DURATION - m_reconTimer);

                if (m_qt_hintWidget)
                {
                    m_qt_hintWidget.SetText(QT_L10n.T("RECON_SCANNING") + "... " + remaining + QT_L10n.T("RECON_REMAINING"));
                    m_qt_hintWidget.Show(!blockQuestTraderShortcut);
                }

                if (m_reconTimer >= RECON_DURATION)
                {
                    m_reconTimer = 0;
                    m_reconActive = false;
                    if (activeRecon == "quest_179") m_recon179Done = true;
                    if (activeRecon == "quest_201") m_recon201Done = true;
                    if (m_qt_hintWidget) { m_qt_hintWidget.SetText(""); m_qt_hintWidget.Show(false); }
                    QT_RPCManager.RequestReconComplete(activeRecon);
                    Print("[QuestTrader] Recon complete for " + activeRecon);
                }
            }
            else if (m_qt_hintWidget)
            {
                string hint = "";
                if (!holdingBinos)
                    hint = QT_L10n.T("RECON_START");
                else
                    hint = QT_L10n.T("RECON_START");

                if (m_reconActive)
                {
                    int rem = Math.Max(0, RECON_DURATION - m_reconTimer);
                    hint = QT_L10n.T("RECON_PAUSED");
                    hint = hint + ": ";
                    hint = hint + rem.ToString();
                    hint = hint + QT_L10n.T("RECON_REMAINING");
                    hint = hint + ". ";
                    hint = hint + QT_L10n.T("RECON_CONTINUE");
                }

                m_qt_hintWidget.SetText(hint);
                m_qt_hintWidget.Show(!blockQuestTraderShortcut);
            }
        }
        else if (m_qt_hintWidget && m_qt_nearbyTraderId == "")
        {
            // Not in any recon zone — hide hint
            if (m_reconActive) { m_qt_hintWidget.Show(false); m_reconActive = false; }
        }
    }

    void QT_CloseQuestMenu()
    {
        if (m_questMenu)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_questMenu);
            m_questMenu = null;
        }
        m_qt_questMenuOpenRequested = false;
        if (m_qt_hintWidget && m_qt_nearbyTraderId != "" && !QT_Input.ShouldBlockQuestTraderShortcut()) m_qt_hintWidget.Show(true);
    }

    void QT_CloseAdminPanel()
    {
        m_qt_adminPanelBlockUntil = GetGame().GetTime() + 1200;
        if (m_adminPanel)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_adminPanel);
            m_adminPanel = null;
        }
        m_qt_adminPanelOpenRequested = false;
    }

    void QT_CloseQuestLog()
    {
        if (m_questLog)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_questLog);
            m_questLog = null;
        }
    }

    void QT_CloseJournal()
    {
        if (m_journal)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_journal);
            m_journal = null;
        }
        if (m_qt_hintWidget && m_qt_nearbyTraderId != "" && !QT_Input.ShouldBlockQuestTraderShortcut()) m_qt_hintWidget.Show(true);
    }

    // Called by QT_DayZGameHook when the client receives an RPC
    void QT_HandleClientRPC(int rpc_type, ParamsReadContext ctx)
    {
        if (!ctx) return;
        if (!QT_RPCGuard.IsQuestTraderRPC(rpc_type)) return;

        if (rpc_type == QT_RPC.TRADER_POSITIONS) { ParseTraderPositions(ctx); return; }
        if (rpc_type == QT_RPC.SEND_QUEST_LIST)    ParseAndShowQuestMenu(ctx);
        else if (rpc_type == QT_RPC.HUD_UPDATE)    ParseAndUpdateHUD(ctx);
        else if (rpc_type == QT_RPC.TOAST)         ParseAndShowToast(ctx);
        else if (rpc_type == QT_RPC.COMPLETE_SOUND) PlayQuestCompleteSound(ctx);
        else if (rpc_type == QT_RPC.READY_SOUND) PlayQuestReadySound(ctx);
        else if (rpc_type == QT_RPC.QUEST_LOG)     ParseAndShowQuestLog(ctx);
        else if (rpc_type == QT_RPC.REQUEST_JOURNAL) ParseAndShowJournal(ctx);
        else if (rpc_type == QT_RPC.ADMIN_PLAYER_DATA) ParseAndShowAdminPlayers(ctx);
        else if (rpc_type == QT_RPC.ADMIN_LOG_DATA)    ParseAndShowAdminLog(ctx);
        else if (rpc_type == QT_RPC.ADMIN_HISTORY_PAGE) ParseAndShowAdminHistoryPage(ctx);
    }

    private void PlayQuestCompleteSound(ParamsReadContext ctx)
    {
        int unused;
        if (!ctx.Read(unused)) return;

        PlayQuestSound(QT_COMPLETE_SOUND_SET, "Complete");
    }

    private void PlayQuestReadySound(ParamsReadContext ctx)
    {
        int unused;
        if (!ctx.Read(unused)) return;

        PlayQuestSound(QT_READY_SOUND_SET, "Ready");
    }

    private bool QT_IsSafeCount(int count, int maxCount)
    {
        return count >= 0 && count <= maxCount;
    }

    private bool QT_IsSafeText(string text, int maxChars)
    {
        return text.Length() <= maxChars;
    }

    private void PlayQuestSound(string soundSet, string label)
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
            return;
        }

        Print("[QuestTrader] " + label + " sound failed: PlaySoundOnObject returned null.");
    }

    private void ParseAndShowQuestMenu(ParamsReadContext ctx)
    {
        string traderId, greeting, traderName;
        int questCount;
        if (!ctx.Read(traderId))    return;
        if (!ctx.Read(greeting))    return;
        if (!ctx.Read(traderName))  return;
        if (!ctx.Read(questCount))  return;
        if (!QT_IsSafeText(traderId, 128)) return;
        if (!QT_IsSafeText(greeting, QT_MAX_TEXT_CHARS)) return;
        if (!QT_IsSafeText(traderName, 256)) return;
        if (!QT_IsSafeCount(questCount, QT_MAX_QUESTS_PER_PACKET)) return;

        ref array<ref QT_QuestEntryUI> quests = new array<ref QT_QuestEntryUI>();

        for (int q = 0; q < questCount; q++)
        {
            ref QT_QuestEntryUI entry = new QT_QuestEntryUI();
            int typeInt, stateInt, objCount, rewCount, cdRemaining;

            if (!ctx.Read(entry.questId)) return;
            if (!ctx.Read(entry.traderId)) return;
            if (!ctx.Read(entry.title)) return;
            if (!ctx.Read(entry.description)) return;
            if (!ctx.Read(typeInt)) return;
            entry.type  = typeInt;
            if (!ctx.Read(stateInt)) return;
            entry.state = stateInt;
            if (!ctx.Read(entry.acceptMessage)) return;
            if (!ctx.Read(entry.rewardMessage)) return;
            if (!ctx.Read(cdRemaining)) return;
            entry.cooldownRemaining = cdRemaining;
            if (!QT_IsSafeText(entry.questId, 128)) return;
            if (!QT_IsSafeText(entry.traderId, 128)) return;
            if (!QT_IsSafeText(entry.title, 256)) return;
            if (!QT_IsSafeText(entry.description, QT_MAX_TEXT_CHARS)) return;
            if (!QT_IsSafeText(entry.acceptMessage, QT_MAX_TEXT_CHARS)) return;
            if (!QT_IsSafeText(entry.rewardMessage, QT_MAX_TEXT_CHARS)) return;

            if (!ctx.Read(objCount)) return;
            if (!QT_IsSafeCount(objCount, QT_MAX_OBJECTIVES_PER_QUEST)) return;
            for (int o = 0; o < objCount; o++)
            {
                string desc; int req, prog;
                if (!ctx.Read(desc)) return;
                if (!ctx.Read(req)) return;
                if (!ctx.Read(prog)) return;
                if (!QT_IsSafeText(desc, QT_MAX_TEXT_CHARS)) return;
                entry.objectiveDescs.Insert(QT_L10n.ResolveText(desc));
                entry.objectiveRequired.Insert(req);
                entry.objectiveProgress.Insert(prog);
            }

            if (!ctx.Read(rewCount)) return;
            if (!QT_IsSafeCount(rewCount, QT_MAX_REWARDS_PER_QUEST)) return;
            for (int r = 0; r < rewCount; r++)
            {
                string rewClass; int rewAmt;
                if (!ctx.Read(rewClass)) return;
                if (!ctx.Read(rewAmt)) return;
                if (!QT_IsSafeText(rewClass, 256)) return;
                entry.rewardDescs.Insert(rewAmt.ToString() + "x " + QT_L10n.ResolveText(rewClass));
            }

            int prereqCount;
            if (!ctx.Read(prereqCount)) return;
            if (!QT_IsSafeCount(prereqCount, QT_MAX_OBJECTIVES_PER_QUEST)) return;
            for (int pr = 0; pr < prereqCount; pr++)
            {
                string preTitle;
                if (!ctx.Read(preTitle)) return;
                if (!QT_IsSafeText(preTitle, 256)) return;
                entry.prereqTitles.Insert(preTitle);
            }

            entry.greeting = greeting;
            quests.Insert(entry);
        }

        // Store trader name and greeting for proximity chat
        m_qt_traderNames.Set(traderId, traderName);
        m_qt_traderGreetings.Set(traderId, greeting);

        if (!m_questMenu && !m_qt_questMenuOpenRequested)
            return;

        if (!m_questMenu)
        {
            m_questMenu = new QT_QuestMenu();
            GetGame().GetUIManager().ShowScriptedMenu(m_questMenu, null);
        }
        m_qt_questMenuOpenRequested = false;
        m_questMenu.SetQuestData(traderId, traderName, quests);
        if (m_qt_hintWidget) m_qt_hintWidget.Show(false);

        // If quest_179 is already COMPLETED, mark recon as done so the
        // hint/timer doesn't activate after a relog
        foreach (QT_QuestEntryUI qEntry : quests)
        {
            if (qEntry.questId == "quest_179" && qEntry.state == QT_QuestState.COMPLETED)
                m_recon179Done = true;
            if (qEntry.questId == "quest_201" && qEntry.state == QT_QuestState.COMPLETED)
                m_recon201Done = true;
        }
    }

    private void ParseAndUpdateHUD(ParamsReadContext ctx)
    {
        int questCount;
        if (!ctx.Read(questCount)) return;
        if (!QT_IsSafeCount(questCount, QT_MAX_QUESTS_PER_PACKET)) return;

        ref array<ref QT_HUDQuestEntry> entries = new array<ref QT_HUDQuestEntry>();
        m_qt_interactQuestIds.Clear();
        m_qt_interactClassNames.Clear();
        m_qt_interactPositions.Clear();
        m_qt_interactDistances.Clear();

        for (int qi = 0; qi < questCount; qi++)
        {
            ref QT_HUDQuestEntry entry = new QT_HUDQuestEntry();
            int stateInt, typeInt, objCount;
            string questId;

            if (!ctx.Read(questId)) return;
            if (!ctx.Read(entry.title)) return;
            if (!ctx.Read(stateInt)) return;
            entry.state = stateInt;
            if (!ctx.Read(typeInt)) return;
            if (!ctx.Read(objCount)) return;
            if (!QT_IsSafeText(questId, 128)) return;
            if (!QT_IsSafeText(entry.title, 256)) return;
            if (!QT_IsSafeCount(objCount, QT_MAX_OBJECTIVES_PER_QUEST)) return;

            for (int o = 0; o < objCount; o++)
            {
                string desc; int req, prog;
                if (!ctx.Read(desc)) return;
                if (!ctx.Read(req)) return;
                if (!ctx.Read(prog)) return;
                if (!QT_IsSafeText(desc, QT_MAX_TEXT_CHARS)) return;
                string tick = "[ ] ";
                if (prog >= req) tick = "[x] ";
                string line = tick + QT_L10n.ResolveText(desc);
                line = line + " (";
                line = line + prog.ToString();
                line = line + "/";
                line = line + req.ToString();
                line = line + ")";
                entry.objectiveLines.Insert(line);
            }

            int hasInteractionTarget;
            string interactionClassName;
            vector interactionPosition;
            float interactionDistance;
            if (!ctx.Read(hasInteractionTarget)) return;
            if (!ctx.Read(interactionClassName)) return;
            if (!ctx.Read(interactionPosition)) return;
            if (!ctx.Read(interactionDistance)) return;
            if (!QT_IsSafeText(interactionClassName, 128)) return;
            if (hasInteractionTarget == 1)
            {
                m_qt_interactQuestIds.Insert(questId);
                m_qt_interactClassNames.Insert(interactionClassName);
                m_qt_interactPositions.Insert(interactionPosition);
                m_qt_interactDistances.Insert(interactionDistance);
            }
            entries.Insert(entry);

            // Detect recon quest completions so timers don't reactivate
            if (questId == "quest_179" && stateInt == QT_QuestState.COMPLETED)
                m_recon179Done = true;
            if (questId == "quest_201" && stateInt == QT_QuestState.COMPLETED)
                m_recon201Done = true;
        }

        if (m_questHUD) m_questHUD.SetEntries(entries);
    }

    private void ParseAndShowToast(ParamsReadContext ctx)
    {
        string message;
        int toastType;
        if (!ctx.Read(message))   return;
        if (!ctx.Read(toastType)) return;
        if (!QT_IsSafeText(message, QT_MAX_TEXT_CHARS)) return;
        QT_Toast.GetInstance().Push(message, toastType);
    }

    private void ParseAndShowQuestLog(ParamsReadContext ctx)
    {
        int histCount;
        if (!ctx.Read(histCount)) return;
        if (!QT_IsSafeCount(histCount, QT_MAX_QUESTS_PER_PACKET)) return;

        ref array<ref QT_QuestLogEntry> history = new array<ref QT_QuestLogEntry>();
        for (int h = 0; h < histCount; h++)
        {
            ref QT_QuestLogEntry e = new QT_QuestLogEntry();
            if (!ctx.Read(e.questId)) return;
            if (!ctx.Read(e.questTitle)) return;
            if (!ctx.Read(e.completedAt)) return;
            if (!ctx.Read(e.rewardSummary)) return;
            if (!ctx.Read(e.runNumber)) return;
            if (!ctx.Read(e.questDescription)) return;
            if (!ctx.Read(e.objectiveSummary)) return;
            if (!QT_IsSafeText(e.questId, 128)) return;
            if (!QT_IsSafeText(e.questTitle, 256)) return;
            if (!QT_IsSafeText(e.completedAt, 128)) return;
            if (!QT_IsSafeText(e.rewardSummary, QT_MAX_TEXT_CHARS)) return;
            if (!QT_IsSafeText(e.questDescription, QT_MAX_TEXT_CHARS)) return;
            if (!QT_IsSafeText(e.objectiveSummary, QT_MAX_TEXT_CHARS)) return;
            history.Insert(e);
        }

        int lbCount;
        if (!ctx.Read(lbCount)) return;
        if (!QT_IsSafeCount(lbCount, QT_MAX_LOG_LINES)) return;
        ref array<string> lb = new array<string>();
        for (int l = 0; l < lbCount; l++)
        {
            string line;
            if (!ctx.Read(line)) return;
            if (!QT_IsSafeText(line, QT_MAX_TEXT_CHARS)) return;
            lb.Insert(line);
        }

        if (m_questLog)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_questLog);
            m_questLog = null;
        }
        m_questLog = new QT_QuestLog();
        GetGame().GetUIManager().ShowScriptedMenu(m_questLog, null);
        m_questLog.SetData(history, lb);
    }

    private void ParseAndShowJournal(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;
        if (!QT_IsSafeCount(count, QT_MAX_QUESTS_PER_PACKET)) return;

        ref array<ref QT_JournalEntry> entries = new array<ref QT_JournalEntry>();
        for (int i = 0; i < count; i++)
        {
            ref QT_JournalEntry e = new QT_JournalEntry();
            if (!ctx.Read(e.title)) return;
            if (!ctx.Read(e.traderName)) return;
            if (!ctx.Read(e.rewardMessage)) return;
            if (!QT_IsSafeText(e.title, 256)) return;
            if (!QT_IsSafeText(e.traderName, 256)) return;
            if (!QT_IsSafeText(e.rewardMessage, QT_MAX_TEXT_CHARS)) return;
            e.description = e.rewardMessage;
            entries.Insert(e);
        }

        if (m_journal)
            QT_CloseJournal();

        m_journal = new QT_Journal();
        GetGame().GetUIManager().ShowScriptedMenu(m_journal, null);
        m_journal.SetJournalData(entries);
    }

    private void ParseAndShowAdminPlayers(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;
        if (!QT_IsSafeCount(count, QT_MAX_PLAYERS_PER_PACKET)) return;

        ref array<ref QT_AdminPlayerEntry> players = new array<ref QT_AdminPlayerEntry>();
        for (int i = 0; i < count; i++)
        {
            ref QT_AdminPlayerEntry p = new QT_AdminPlayerEntry();
            if (!ctx.Read(p.uid)) return;
            if (!ctx.Read(p.name)) return;
            if (!ctx.Read(p.steamId)) return;
            if (!ctx.Read(p.activeQuests)) return;
            if (!ctx.Read(p.totalCompleted)) return;
            if (!ctx.Read(p.detailText)) return;
            if (!QT_IsSafeText(p.uid, 128)) return;
            if (!QT_IsSafeText(p.name, 256)) return;
            if (!QT_IsSafeText(p.steamId, 128)) return;
            if (!QT_IsSafeText(p.detailText, QT_MAX_TEXT_CHARS)) return;
            if (p.name == "" && p.uid == "") continue;
            if (p.activeQuests < 0 || p.activeQuests > 9999) p.activeQuests = 0;
            if (p.totalCompleted < 0 || p.totalCompleted > 999999) p.totalCompleted = 0;
            players.Insert(p);
        }

        if (!m_adminPanel && (!m_qt_adminPanelOpenRequested || GetGame().GetTime() < m_qt_adminPanelBlockUntil))
            return;

        if (!m_adminPanel)
        {
            m_adminPanel = new QT_AdminPanel();
            GetGame().GetUIManager().ShowScriptedMenu(m_adminPanel, null);
        }
        m_qt_adminPanelOpenRequested = false;
        m_adminPanel.SetPlayerData(players);
    }


    private string QT_GetInteractHint()
    {
        return "[ " + QT_Input.GetBoundKeyName(QT_INPUT_OPEN_QUEST_MENU) + " ] " + QT_L10n.T("HINT_INTERACT_ACTION");
    }

    private string QT_GetObjectInteractHint()
    {
        return "[ " + QT_Input.GetBoundKeyName(QT_INPUT_OPEN_QUEST_MENU) + " ] " + QT_L10n.T("HINT_INTERACT_OBJECT");
    }

    private void UpdateNearbyInteraction(PlayerBase player, float timeslice, bool blockQuestTraderShortcut)
    {
        m_qt_interactScanTimer += timeslice;
        if (m_qt_interactScanTimer < 0.25) return;
        m_qt_interactScanTimer = 0;

        string questId = "";
        string className = "";
        vector objectPos = vector.Zero;
        array<Object> foundObjects = new array<Object>();
        array<CargoBase> proxyCargos = new array<CargoBase>();

        for (int ii = 0; ii < m_qt_interactQuestIds.Count(); ii++)
        {
            string neededClass = m_qt_interactClassNames[ii];
            vector neededPos = m_qt_interactPositions[ii];
            float neededDist = m_qt_interactDistances[ii];
            if (neededDist <= 0) neededDist = 3.0;

            bool positionOk = false;
            if (neededPos != vector.Zero)
            {
                positionOk = vector.Distance(player.GetPosition(), neededPos) <= neededDist;
                if (positionOk) objectPos = neededPos;
            }

            bool classOk = true;
            if (neededClass != "")
            {
                classOk = false;
                foundObjects.Clear();
                proxyCargos.Clear();
                GetGame().GetObjectsAtPosition(player.GetPosition(), neededDist, foundObjects, proxyCargos);

                foreach (Object foundObj : foundObjects)
                {
                    if (!foundObj) continue;

                    string foundType = foundObj.GetType();
                    if (foundType == "") continue;
                    if (!foundType.Contains(neededClass) && !GetGame().IsKindOf(foundType, neededClass)) continue;

                    vector foundPos = foundObj.GetPosition();
                    if (neededPos != vector.Zero && vector.Distance(foundPos, neededPos) > neededDist) continue;

                    classOk = true;
                    className = foundType;
                    objectPos = foundPos;
                    break;
                }
            }

            if ((neededPos == vector.Zero || positionOk) && classOk)
            {
                questId = m_qt_interactQuestIds[ii];
                if (className == "") className = neededClass;
                break;
            }
        }

        if (questId != m_qt_nearbyInteractionQuestId)
        {
            if (m_qt_hintWidget && m_qt_nearbyTraderId == "" && !m_questMenu && !m_journal && !blockQuestTraderShortcut)
            {
                if (questId != "") m_qt_hintWidget.SetText(QT_GetObjectInteractHint());
                m_qt_hintWidget.Show(questId != "");
            }
        }

        if (m_qt_hintWidget && m_qt_nearbyTraderId == "" && questId != "" && !m_questMenu && !m_journal && !blockQuestTraderShortcut)
            m_qt_hintWidget.SetText(QT_GetObjectInteractHint());

        m_qt_nearbyInteractionQuestId = questId;
        m_qt_nearbyInteractionClassName = className;
        m_qt_nearbyInteractionPosition = objectPos;
    }

    // Called by OnUpdate - check for F key when near a trader
    private void ParseTraderPositions(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;
        if (!QT_IsSafeCount(count, QT_MAX_TRADER_POSITIONS)) return;
        m_qt_traderIds       = new array<string>();
        m_qt_traderPositions = new array<vector>();
        for (int i = 0; i < count; i++)
        {
            string tid; vector pos; float dist; string tName; string greeting;
            if (!ctx.Read(tid)) return;
            if (!ctx.Read(pos)) return;
            if (!ctx.Read(dist)) return;
            if (!ctx.Read(tName)) return;
            if (!ctx.Read(greeting)) return;
            if (!QT_IsSafeText(tid, 128)) return;
            if (!QT_IsSafeText(tName, 256)) return;
            if (!QT_IsSafeText(greeting, QT_MAX_TEXT_CHARS)) return;
            m_qt_traderIds.Insert(tid);
            m_qt_traderPositions.Insert(pos);
            // Only set maxDist from first trader and clamp to sane range (1-15m)
            if (i == 0)
            {
                m_qt_maxDist = Math.Clamp(dist, 1.0, 15.0);
            }
            // Pre-populate name and greeting maps so they're ready before menu opens
            m_qt_traderNames.Set(tid, tName);
            m_qt_traderGreetings.Set(tid, greeting);
        }
        Print("[QuestTrader] Client received " + count + " trader positions.");
    }

    private void ParseAndShowAdminLog(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;
        if (!QT_IsSafeCount(count, QT_MAX_LOG_LINES)) return;

        ref array<string> lines = new array<string>();
        for (int i = 0; i < count; i++)
        {
            string line;
            if (!ctx.Read(line)) return;
            if (!QT_IsSafeText(line, QT_MAX_TEXT_CHARS)) return;
            lines.Insert(line);
        }

        if (m_adminPanel) m_adminPanel.SetLogLines(lines);
    }

    private void ParseAndShowAdminHistoryPage(ParamsReadContext ctx)
    {
        string uid;
        int page;
        int totalPages;
        int totalEntries;
        int lineCount;
        if (!ctx.Read(uid)) return;
        if (!ctx.Read(page)) return;
        if (!ctx.Read(totalPages)) return;
        if (!ctx.Read(totalEntries)) return;
        if (!ctx.Read(lineCount)) return;
        if (!QT_IsSafeText(uid, 128)) return;
        if (page < 0 || totalPages < 0 || totalEntries < 0) return;
        if (!QT_IsSafeCount(lineCount, QT_MAX_LOG_LINES)) return;

        ref array<string> lines = new array<string>();
        for (int i = 0; i < lineCount; i++)
        {
            string line;
            if (!ctx.Read(line)) return;
            if (!QT_IsSafeText(line, QT_MAX_TEXT_CHARS)) return;
            lines.Insert(line);
        }

        if (m_adminPanel) m_adminPanel.SetHistoryPage(uid, page, totalPages, totalEntries, lines);
    }

}
